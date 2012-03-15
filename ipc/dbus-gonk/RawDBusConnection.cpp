/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RawDBusConnection.h"
#include <dbus/dbus.h>
#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"

using namespace mozilla::ipc;

RawDBusConnection::RawDBusConnection() {
	createDBusConnection();
}

RawDBusConnection::~RawDBusConnection() {
}

bool RawDBusConnection::createDBusConnection() {
  DBusError err;
  dbus_error_init(&err);
  dbus_threads_init_default();
  mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return false;
  }
  dbus_connection_set_exit_on_disconnect(mConnection, FALSE);
  return true;
}

// If err is NULL, then any errors will be LOGE'd, and free'd and the reply
// will be NULL.
// If err is not NULL, then it is assumed that dbus_error_init was already
// called, and error's will be returned to the caller without logging. The
// return value is NULL iff an error was set. The client must free the error if
// set.
DBusMessage * RawDBusConnection::dbus_func_args_timeout_valist(int timeout_ms,
                                                               DBusError *err,
                                                               const char *path,
                                                               const char *ifc,
                                                               const char *func,
                                                               int first_arg_type,
                                                               va_list args) {

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
  reply = dbus_connection_send_with_reply_and_block(mConnection, msg, timeout_ms, err);
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

DBusMessage * RawDBusConnection::dbus_func_args_timeout(int timeout_ms,
                                                        const char *path,
                                                        const char *ifc,
                                                        const char *func,
                                                        int first_arg_type,
                                                        ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(timeout_ms, NULL,
                                      path, ifc, func,
                                      first_arg_type, lst);
  va_end(lst);
  return ret;
}

DBusMessage * RawDBusConnection::dbus_func_args(                           
  const char *path,
  const char *ifc,
  const char *func,
  int first_arg_type,
  ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(-1, NULL,
                                      path, ifc, func,
                                      first_arg_type, lst);
  va_end(lst);
  return ret;
}

DBusMessage * RawDBusConnection::dbus_func_args_error(DBusError *err,
                                                      const char *path,
                                                      const char *ifc,
                                                      const char *func,
                                                      int first_arg_type,
                                                      ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(-1, err,
                                      path, ifc, func,
                                      first_arg_type, lst);
  va_end(lst);
  return ret;
}

int32_t RawDBusConnection::dbus_returns_int32(DBusMessage *reply) {

  DBusError err;
  int ret = -1;

  dbus_error_init(&err);
  if (!dbus_message_get_args(reply, &err,
                             DBUS_TYPE_INT32, &ret,
                             DBUS_TYPE_INVALID)) {
    //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
  }
  dbus_message_unref(reply);
  return ret;
}

uint32_t RawDBusConnection::dbus_returns_uint32(DBusMessage *reply) {

  DBusError err;
  int ret = -1;

  dbus_error_init(&err);
  if (!dbus_message_get_args(reply, &err,
                             DBUS_TYPE_UINT32, &ret,
                             DBUS_TYPE_INVALID)) {
    //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
  }
  dbus_message_unref(reply);
  return ret;
}

// nsString RawDBusConnection::dbus_returns_string(DBusMessage *reply) {

//   DBusError err;
//   nsString ret = NS_LITERAL_STRING("test");
//   const char *name;

//   dbus_error_init(&err);
//   if (dbus_message_get_args(reply, &err,
//                             DBUS_TYPE_STRING, &name,
//                             DBUS_TYPE_INVALID)) {
//     //ret = env->NewStringUTF(name);
//   } else {
//     //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
//   }
//   dbus_message_unref(reply);

//   return ret;
// }

bool RawDBusConnection::dbus_returns_boolean(DBusMessage *reply) {
  DBusError err;
  bool ret = false;
  dbus_bool_t val = false;

  dbus_error_init(&err);

  /* Check the return value. */
  if (dbus_message_get_args(reply, &err,
                            DBUS_TYPE_BOOLEAN, &val,
                            DBUS_TYPE_INVALID)) {
    ret = val == true ? true : false;
  } else {
    //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
  }

  dbus_message_unref(reply);
  return ret;
}
