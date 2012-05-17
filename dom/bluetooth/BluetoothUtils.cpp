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

#include "BluetoothUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>

#include "dbus/dbus.h"
#include "mozilla/ipc/DBusUtils.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

using namespace mozilla::ipc;

BEGIN_BLUETOOTH_NAMESPACE

struct Properties {
  char* name;
  int type;
};

static Properties remote_device_properties[] = {
  {"Address",  DBUS_TYPE_STRING},
  {"Name", DBUS_TYPE_STRING},
  {"Icon", DBUS_TYPE_STRING},
  {"Class", DBUS_TYPE_UINT32},
  {"UUIDs", DBUS_TYPE_ARRAY},
  {"Services", DBUS_TYPE_ARRAY},
  {"Paired", DBUS_TYPE_BOOLEAN},
  {"Connected", DBUS_TYPE_BOOLEAN},
  {"Trusted", DBUS_TYPE_BOOLEAN},
  {"Blocked", DBUS_TYPE_BOOLEAN},
  {"Alias", DBUS_TYPE_STRING},
  {"Nodes", DBUS_TYPE_ARRAY},
  {"Adapter", DBUS_TYPE_OBJECT_PATH},
  {"LegacyPairing", DBUS_TYPE_BOOLEAN},
  {"RSSI", DBUS_TYPE_INT16},
  {"TX", DBUS_TYPE_UINT32},
  {"Broadcaster", DBUS_TYPE_BOOLEAN}
};

static Properties adapter_properties[] = {
  {"Address", DBUS_TYPE_STRING},
  {"Name", DBUS_TYPE_STRING},
  {"Class", DBUS_TYPE_UINT32},
  {"Powered", DBUS_TYPE_BOOLEAN},
  {"Discoverable", DBUS_TYPE_BOOLEAN},
  {"DiscoverableTimeout", DBUS_TYPE_UINT32},
  {"Pairable", DBUS_TYPE_BOOLEAN},
  {"PairableTimeout", DBUS_TYPE_UINT32},
  {"Discovering", DBUS_TYPE_BOOLEAN},
  {"Devices", DBUS_TYPE_ARRAY},
  {"UUIDs", DBUS_TYPE_ARRAY},
};

typedef union {
  char *str_val;
  int int_val;
  char **array_val;
} property_value;

int get_property(DBusMessageIter iter, Properties *properties,
                 int max_num_properties, int *prop_index, property_value *value, int *len) {
  DBusMessageIter prop_val, array_val_iter;
  char *property = NULL;
  uint32_t array_type;
  char *str_val;
  int i, j, type, int_val;

  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
    return -1;
  dbus_message_iter_get_basic(&iter, &property);
  if (!dbus_message_iter_next(&iter))
    return -1;
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
    return -1;
  for (i = 0; i <  max_num_properties; i++) {
    if (!strncmp(property, properties[i].name, strlen(property)))
      break;
  }
  *prop_index = i;
  if (i == max_num_properties)
    return -1;

  dbus_message_iter_recurse(&iter, &prop_val);
  type = properties[*prop_index].type;
  if (dbus_message_iter_get_arg_type(&prop_val) != type) {
    LOG("Property type mismatch in get_property: %d, expected:%d, index:%d",
        dbus_message_iter_get_arg_type(&prop_val), type, *prop_index);
    return -1;
  }

  switch(type) {
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
      dbus_message_iter_get_basic(&prop_val, &value->str_val);
      *len = 1;
      break;
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_BOOLEAN:
      dbus_message_iter_get_basic(&prop_val, &int_val);
      value->int_val = int_val;
      *len = 1;
      break;
    case DBUS_TYPE_ARRAY:
      dbus_message_iter_recurse(&prop_val, &array_val_iter);
      array_type = dbus_message_iter_get_arg_type(&array_val_iter);
      *len = 0;
      value->array_val = NULL;
      if (array_type == DBUS_TYPE_OBJECT_PATH ||
          array_type == DBUS_TYPE_STRING){
        j = 0;
        do {
          j ++;
        } while(dbus_message_iter_next(&array_val_iter));
        dbus_message_iter_recurse(&prop_val, &array_val_iter);
        // Allocate  an array of char *
        *len = j;
        char **tmp = (char **)malloc(sizeof(char *) * *len);
        if (!tmp)
          return -1;
        j = 0;
        do {
          dbus_message_iter_get_basic(&array_val_iter, &tmp[j]);
          j ++;
        } while(dbus_message_iter_next(&array_val_iter));
        value->array_val = tmp;
      }
      break;
    default:
      return -1;
  }
  return 0;
}

void create_prop_array(std::list<const char*>& strArray, Properties *property,
    property_value *value, int len, int *array_index ) {
  char **prop_val = NULL;
  char buf[32] = {'\0'}, buf1[32] = {'\0'};
  int i;

  char *name = property->name;
  int prop_type = property->type;

  strArray.push_back(name);

  if (prop_type == DBUS_TYPE_UINT32 || prop_type == DBUS_TYPE_INT16) {
    sprintf(buf, "%d", value->int_val);
    strArray.push_back(buf);
  } else if (prop_type == DBUS_TYPE_BOOLEAN) {
    sprintf(buf, "%s", value->int_val ? "true" : "false");
    strArray.push_back(buf);
  } else if (prop_type == DBUS_TYPE_ARRAY) {
    // Write the length first
    sprintf(buf1, "%d", len);
    strArray.push_back(buf1);

    prop_val = value->array_val;
    for (i = 0; i < len; i++) {
      strArray.push_back(prop_val[i]);
    }
  } else {
    strArray.push_back(value->str_val);
  }
}

std::list<const char*> parse_property_change(DBusMessage *msg,
                           Properties *properties, 
                           int max_num_properties) {
  DBusMessageIter iter;
  DBusError err;
  int len = 0, prop_index = -1;
  int array_index = 0, size = 0;
  property_value value;
  std::list<const char*> strArray;

  dbus_error_init(&err);
  if (!dbus_message_iter_init(msg, &iter))
    goto failure;

  if (!get_property(iter, properties, max_num_properties,
        &prop_index, &value, &len)) {
    size += 2;
    if (properties[prop_index].type == DBUS_TYPE_ARRAY)
      size += len;

    create_prop_array(strArray, &properties[prop_index],
                      &value, len, &array_index);

    if (properties[prop_index].type == DBUS_TYPE_ARRAY && value.array_val != NULL)
      free(value.array_val);

    return strArray;
  }

failure:
  LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
  return strArray;
}

std::list<const char*> parse_adapter_property_change(DBusMessage *msg) {
  return parse_property_change(msg, 
                               (Properties *) &adapter_properties,
                               sizeof(adapter_properties) / sizeof(Properties));
}

int get_bdaddr(const char *str, bdaddr_t *ba) 
{
  char *d = ((char *)ba) + 5, *endp;
  int i;
  for(i = 0; i < 6; i++) {
    *d-- = strtol(str, &endp, 16);
    if (*endp != ':' && i != 5) {
      memset(ba, 0, sizeof(bdaddr_t));
      return -1;
    }
    str = endp + 1;
  }
  return 0;
}

void get_bdaddr_as_string(const bdaddr_t *ba, char *str) 
{
  const uint8_t *b = (const uint8_t *)ba;
  sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
      b[5], b[4], b[3], b[2], b[1], b[0]);
}

END_BLUETOOTH_NAMESPACE
