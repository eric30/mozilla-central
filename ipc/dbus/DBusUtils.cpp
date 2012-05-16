/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include "dbus/dbus.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args);
#else
#define LOG(args...)  printf(args);
#endif

#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"

typedef struct {
    void (*user_cb)(DBusMessage *, void *, void *);
      void *user;
} dbus_async_call_t;

namespace mozilla {
namespace ipc {

const char * get_adapter_path(DBusConnection *conn)
{
  DBusMessage *msg = NULL, *reply = NULL;
  DBusError err;
  const char *device_path = NULL;
  int attempt = 0;

  for (attempt = 0; attempt < 1000 && reply == NULL; attempt ++) {
    msg = dbus_message_new_method_call("org.bluez", "/",
        "org.bluez.Manager", "DefaultAdapter");
    if (!msg) {
      LOG("%s: Can't allocate new method call for get_adapter_path!",
          __FUNCTION__);
      return NULL;
    }
    dbus_message_append_args(msg, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);

    if (!reply) {
      if (dbus_error_is_set(&err)) {
        if (dbus_error_has_name(&err,
              "org.freedesktop.DBus.Error.ServiceUnknown")) {
          // bluetoothd is still down, retry
          LOG("Service unknown\n");
          dbus_error_free(&err);
          //usleep(10000);  // 10 ms
          continue;
        } else {
          // Some other error we weren't expecting
          LOG("other error\n");
          dbus_error_free(&err);
        }
      }
    }
  }

  if (attempt == 1000) {
    LOG("timeout\n");
    goto failed;
  }

  if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH,
        &device_path, DBUS_TYPE_INVALID)
      || !device_path) {
    if (dbus_error_is_set(&err)) {
      dbus_error_free(&err);
    }
    goto failed;
  }
  dbus_message_unref(msg);
  LOG("Adapter path: %s\n", device_path);

  return device_path;

failed:
  dbus_message_unref(msg);
  return NULL;
}

void
log_and_free_dbus_error(DBusError* err, const char* function, DBusMessage* msg)
{
  if(msg) {
    LOG("%s: D-Bus error in %s: %s (%s)", function,
        dbus_message_get_member((msg)), (err)->name, (err)->message);
  }	else {
    LOG("%s: D-Bus error: %s (%s)", __FUNCTION__,
        (err)->name, (err)->message);
  }
  dbus_error_free((err));
}

DBusMessage * dbus_func_args_timeout_valist(DBusConnection* conn,
                                            int timeout_ms,
                                            DBusError *err,
                                            const char *path,
                                            const char *ifc,
                                            const char *func,
                                            int first_arg_type,
                                            va_list args) 
{
  DBusMessage *msg = NULL, *reply = NULL;
  const char *name;
  bool return_error = (err != NULL);

  if (!return_error) {
    err = (DBusError*)malloc(sizeof(DBusError));
    dbus_error_init(err);
  }

  /* Compose the command */
  msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, path, ifc, func);

  if (msg == NULL) {
    //LOGE("Could not allocate D-Bus message object!");
    goto done;
  }
  /* append arguments */
  if (!dbus_message_append_args_valist(msg, first_arg_type, args)) {
    //LOGE("Could not append argument to method call!");
    goto done;
  }

  /* Make the call. */
  reply = dbus_connection_send_with_reply_and_block(conn, msg, timeout_ms, err);
  if (!return_error && dbus_error_is_set(err)) {
    //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(err, msg);
  }

done:
  if (!return_error) {
    free(err);
  }
  if (msg) dbus_message_unref(msg);
  return reply;
}

DBusMessage * dbus_func_args_timeout(DBusConnection* conn,
                                     int timeout_ms,
                                     const char *path,
                                     const char *ifc,
                                     const char *func,
                                     int first_arg_type,
                                     ...) 
{
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(conn, timeout_ms, NULL,
      path, ifc, func,
      first_arg_type, lst);
  va_end(lst);
  return ret;
}

DBusMessage * dbus_func_args(DBusConnection* conn,
                             const char *path,
                             const char *ifc,
                             const char *func,
                             int first_arg_type,
                             ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(conn, -1, NULL,
      path, ifc, func,
      first_arg_type, lst);
  va_end(lst);
  return ret;
}

void dbus_func_args_async_callback(DBusPendingCall *call, void *data)
{
  dbus_async_call_t *req = (dbus_async_call_t *)data;
  DBusMessage *msg;

  // This is guaranteed to be non-NULL, because this function is called only
  // when once the remote method invokation returns. 
  msg = dbus_pending_call_steal_reply(call);

  if (msg) {
    if (req->user_cb) {
      // The user may not deref the message object.
      req->user_cb(msg, req->user, NULL);
    }
    dbus_message_unref(msg);
  }

  //dbus_message_unref(req->method);
  dbus_pending_call_cancel(call);
  dbus_pending_call_unref(call);
  free(req);
}


bool dbus_func_args_async_valist(DBusConnection* conn,
                                 int timeout_ms,
                                 void (*user_cb)(DBusMessage *, void *, void*),
                                 void *user,
                                 const char *path,
                                 const char *ifc,
                                 const char *func,
                                 int first_arg_type,
                                 va_list args)
{
  DBusMessage *msg = NULL;
  const char *name;
  dbus_bool_t reply = FALSE;
  dbus_async_call_t *pending;

  /* Compose the command */
  msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, path, ifc, func);

  if (msg == NULL) {
    LOG("Could not allocate D-Bus message object!");
    goto done;
  }

  /* append arguments */
  if (!dbus_message_append_args_valist(msg, first_arg_type, args)) {
    LOG("Could not append argument to method call!");
    goto done;
  }

  /* Make the call. */
  pending = (dbus_async_call_t *)malloc(sizeof(dbus_async_call_t));
  if (pending) {
    DBusPendingCall *call;

    pending->user = user;
    pending->user_cb = user_cb;

    reply = dbus_connection_send_with_reply(conn, msg, &call,
        timeout_ms);
    if (reply == TRUE) {
      dbus_pending_call_set_notify(call,
          dbus_func_args_async_callback,
          pending,
          NULL);
    }
  }

done:
  if (msg) dbus_message_unref(msg);
    return reply ? true : false;
}

bool dbus_func_args_async(DBusConnection* conn,
                          int timeout_ms,
                          void (*reply)(DBusMessage *, void *, void*),
                          void *user,
                          const char *path,
                          const char *ifc,
                          const char *func,
                          int first_arg_type,
                          ...) 
{
  bool ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_async_valist(conn, 
      timeout_ms,
      reply, user,
      path, ifc, func,
      first_arg_type, lst);
  va_end(lst);
  return ret;
}

}
}
