/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_dbus_gonk_rawdbusconnection_h__
#define mozilla_ipc_dbus_gonk_rawdbusconnection_h__

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>

struct DBusMessage;
struct DBusError;
struct DBusConnection;

namespace mozilla {
namespace ipc {

class RawDBusConnection
{
public:
  RawDBusConnection();
  ~RawDBusConnection();
  bool createDBusConnection();

  DBusMessage * dbus_func_args_timeout_valist(
    int timeout_ms,
    DBusError *err,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    va_list args);

  DBusMessage * dbus_func_args_timeout(
    int timeout_ms,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    ...);

  DBusMessage * dbus_func_args(                           
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    ...);

  DBusMessage * dbus_func_args_error(
    DBusError *err,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    ...);

  bool dbus_func_args_async_valist(
      int timeout_ms,
      void (*user_cb)(DBusMessage *,
        void *,
        void*),
      void *user,
      const char *path,
      const char *ifc,
      const char *func,
      int first_arg_type,
      va_list args);

  bool dbus_func_args_async(
      int timeout_ms,
      void (*reply)(DBusMessage *, void *, void*),
      void *user,
      const char *path,
      const char *ifc,
      const char *func,
      int first_arg_type,
      ...);

  int32_t dbus_returns_int32(DBusMessage *reply);
  uint32_t dbus_returns_uint32(DBusMessage *reply);
  // std::string dbus_returns_string(DBusMessage *reply);
  bool dbus_returns_boolean(DBusMessage *reply);

protected:
  DBusConnection* mConnection;  
};

}
}

#endif
