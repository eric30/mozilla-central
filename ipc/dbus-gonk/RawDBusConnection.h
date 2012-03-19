/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_dbus_gonk_rawdbusconnection_h__
#define mozilla_ipc_dbus_gonk_rawdbusconnection_h__

#include <string.h>
#include <stdint.h>
//#include <dbus/dbus.h>
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

  int32_t dbus_returns_int32(DBusMessage *reply);
  uint32_t dbus_returns_uint32(DBusMessage *reply);
  // std::string dbus_returns_string(DBusMessage *reply);
  bool dbus_returns_boolean(DBusMessage *reply);

  // template<typename T>
  // bool GetDBusDictValue(DBusMessageIter& iter, const char* name, int expected_type, T& val) {
  //   DBusMessageIter dict_entry, dict;
  //   int size = 0,array_index = 0;
  //   int len = 0, prop_type = DBUS_TYPE_INVALID, prop_index = -1, type;

  //   if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
  //     printf("This isn't a dictionary!\n");
  //     return false;
  //   }
  //   dbus_message_iter_recurse(&iter, &dict);
  //   do {
  //     len = 0;
  //     if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_DICT_ENTRY)
  //     {
  //       printf("Not a dict entry!\n");
  //       return false;
  //     }            
  //     dbus_message_iter_recurse(&dict, &dict_entry);
  //     DBusMessageIter prop_val, array_val_iter;
  //     char *property = NULL;
  //     uint32_t array_type;
  //     char *str_val;
  //     int i, j, type, int_val;

  //     if (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_STRING) {
  //       printf("Iter not a string!\n");
  //       return false;
  //     }
  //     dbus_message_iter_get_basic(&dict_entry, &property);
  //     printf("Property: %s\n", property);
  //     if(strcmp(property, name) != 0) continue;
  //     if (!dbus_message_iter_next(&dict_entry))
  //     {
  //       printf("No next!\n");
  //       return false;
  //     }
  //     if (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_VARIANT)
  //     {
  //       printf("Iter not a variant!\n");
  //       return false;
  //     }
  //     dbus_message_iter_recurse(&dict_entry, &prop_val);
  //     // type = properties[*prop_index].type;
  //     if (dbus_message_iter_get_arg_type(&prop_val) != expected_type) {
  //       //   LOGE("Property type mismatch in get_property: %d, expected:%d, index:%d",
  //       //        dbus_message_dict_entry_get_arg_type(&prop_val), type, *prop_index);
  //       printf("Not the type we expect!\n");
  //       return false;
  //     }
  //     dbus_message_iter_get_basic(&prop_val, &val);
  //     return true;
  //   } while(dbus_message_iter_next(&dict));
  //   return false;
  // }

protected:
  DBusConnection* mConnection;  
};

}
}

#endif
