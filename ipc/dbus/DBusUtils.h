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

#ifndef mozilla_ipc_dbus_dbusutils_h__
#define mozilla_ipc_dbus_dbusutils_h__

#include <stdarg.h>

// LOGE and free a D-Bus error
// Using #define so that __FUNCTION__ resolves usefully
#define LOG_AND_FREE_DBUS_ERROR_WITH_MSG(err, msg) log_and_free_dbus_error(err, __FUNCTION__, msg);
#define LOG_AND_FREE_DBUS_ERROR(err) log_and_free_dbus_error(err, __FUNCTION__);

struct DBusMessage;
struct DBusError;
struct DBusConnection;

namespace mozilla {
namespace ipc {
const char * get_adapter_path(DBusConnection *conn);
void log_and_free_dbus_error(DBusError* err, const char* function, DBusMessage* msg = NULL);
uint32_t dbus_returns_uint32(DBusMessage *reply);
int32_t dbus_returns_int32(DBusMessage *reply);
bool dbus_returns_boolean(DBusMessage *reply);

DBusMessage * dbus_func_args_timeout_valist(
    DBusConnection* conn,
    int timeout_ms,
    DBusError *err,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    va_list args);

DBusMessage * dbus_func_args_timeout(
    DBusConnection* conn,
    int timeout_ms,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    ...);

DBusMessage * dbus_func_args(
    DBusConnection* conn,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    ...);

bool dbus_func_args_async(DBusConnection* conn,
    int timeout_ms,
    void (*reply)(DBusMessage *, void *, void*),
    void *user,
    const char *path,
    const char *ifc,
    const char *func,
    int first_arg_type,
    ...);

}
}

#endif

