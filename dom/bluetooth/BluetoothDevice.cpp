/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDevice.h"
#include "BluetoothCommon.h"
#include "BluetoothProperties.h"
#include "BluetoothSocket.h"
#include "BluetoothServiceUuid.h"
#include "BluetoothHfpManager.h"

#include "AudioManager.h"
#include "dbus/dbus.h"
#include "nsDOMClassInfo.h"
#include "nsString.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothDevice, BluetoothDevice)

NS_INTERFACE_MAP_BEGIN(BluetoothDevice)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothDevice)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMBluetoothDevice)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothDevice)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(BluetoothDevice)
NS_IMPL_RELEASE(BluetoothDevice)

BluetoothDevice::BluetoothDevice(const char* aAddress, 
                                 const char* aName,
                                 const char* aObjectPath)
{
  mAddress = NS_ConvertASCIItoUTF16(aAddress);
  mName = NS_ConvertASCIItoUTF16(aName);

  if (!aObjectPath) {
    LOG("OBJECT PATH is NULL in ctor of BluetoothDevice");
  } else {
    mObjectPath = (char *)calloc(strlen(aObjectPath), sizeof(char));
    strcpy(mObjectPath, aObjectPath);

    UpdateProperties();
  }
}

nsresult
BluetoothDevice::HandleEvent(DBusMessage* msg)
{
  // TODO(Eric)
  // Currently, I'm still not sure if we're gonna implement an event handler in BluetoothDevice or not.

  return NS_OK;
}

nsresult
BluetoothDevice::GetAdapter(nsAString& aAdapter)
{
  aAdapter = mAdapter;
  return NS_OK;
}

nsresult
BluetoothDevice::GetAddress(nsAString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}

nsresult
BluetoothDevice::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

nsresult
BluetoothDevice::GetClass(PRUint32* aClass)
{
  *aClass = mClass;
  return NS_OK;
}

nsresult
BluetoothDevice::GetConnected(bool* aConnected)
{
  UpdateProperties();
  *aConnected = mConnected;
  return NS_OK;
}

nsresult
BluetoothDevice::GetPaired(bool* aPaired)
{
  *aPaired = mPaired;
  return NS_OK;
}

nsresult
BluetoothDevice::GetLegacyPairing(bool* aLegacyPairing)
{
  *aLegacyPairing = mLegacyPairing;
  return NS_OK;
}

nsresult
BluetoothDevice::GetTrusted(bool* aTrusted)
{
  *aTrusted = mTrusted;
  return NS_OK;
}

nsresult
BluetoothDevice::SetTrusted(bool aTrusted)
{
  if(aTrusted == mTrusted) return NS_OK;

  int value = aTrusted ? 1 : 0;

  if (!SetProperty("Trusted", DBUS_TYPE_BOOLEAN, (void*)&value)) {
    return NS_ERROR_FAILURE;
  }

  mTrusted = aTrusted;

  return NS_OK;
}

nsresult
BluetoothDevice::GetAlias(nsAString& aAlias)
{
  aAlias = mAlias;
  return NS_OK;
}

nsresult
BluetoothDevice::SetAlias(const nsAString& aAlias)
{
  if (mAlias.Equals(aAlias)) return NS_OK;

  const char* asciiAlias = ToNewCString(aAlias);

  if (!SetProperty("Alias", DBUS_TYPE_STRING, (void*)&asciiAlias)) {
    return NS_ERROR_FAILURE;
  }

  mAlias = aAlias;

  return NS_OK;
}

nsresult
BluetoothDevice::GetUuids(jsval* aUuids)
{
  //TODO: convert mUuids to jsval and assign to aUuids;
  return NS_OK;
}

nsresult
BluetoothDevice::Connect(PRInt32 channel, bool* success)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(mAddress).get();

  // TODO(Eric)
  // Should dispatch to diff profiles according to requesting profiles
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  *success = hfp->Connect(channel, asciiAddress);

  return NS_OK;
}

nsresult
BluetoothDevice::Disconnect(bool* success)
{
  // TODO(Eric)
  // Should find each profile to see if this device has a connection
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  hfp->Disconnect();
  *success = true;

  return NS_OK;
}

static void
get_property(DBusMessageIter dict_entry, BluetoothProperties* properties,
             int* ret_property_index, property_value* ret_property_value, int* ret_length)
{
  DBusMessageIter prop_value, array_value;

  int type = dbus_message_iter_get_arg_type(&dict_entry);

  if (type != DBUS_TYPE_STRING) {
    LOG("Type = %d, not DBUS_TYPE_STRING\n", type);
  } else {
    char* property_name;
    dbus_message_iter_get_basic(&dict_entry, &property_name);
    LOG("Property Name:%s, ", property_name);

    if (!dbus_message_iter_next(&dict_entry)) {
      LOG("No next item.");
    } else {
      if (dbus_message_iter_get_arg_type(&dict_entry) == DBUS_TYPE_VARIANT) {
        // Search for the matching property and get its type
        int property_type = DBUS_TYPE_INVALID;
        int property_size = sizeof(remote_device_properties) / sizeof(BluetoothProperties);
        int i;

        for (i = 0;i < property_size;++i)
        {
          if (!strncmp(property_name, properties[i].mPropertyName, strlen(property_name)))
          {
            property_type = properties[i].mType;
            break;
          }
        }

        if (property_type == DBUS_TYPE_INVALID)
        {
          *ret_property_index = -1;
          LOG("No matching property");
        } else {
          *ret_property_index = i;
          dbus_message_iter_recurse(&dict_entry, &prop_value);

          int value, array_type;
          char* str;

          switch(property_type)
          {
            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
              dbus_message_iter_get_basic(&prop_value, &str);
              LOG("Value : %s\n", str);
              ret_property_value->str_val = str;
              break;
            case DBUS_TYPE_UINT32:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_BOOLEAN:
              dbus_message_iter_get_basic(&prop_value, &value);
              LOG("Value : %d\n", value);
              ret_property_value->int_val = value;
              break;

            case DBUS_TYPE_ARRAY:
              dbus_message_iter_recurse(&prop_value, &array_value);
              array_type = dbus_message_iter_get_arg_type(&array_value);

              *ret_length = 0;

              if (array_type == DBUS_TYPE_OBJECT_PATH ||
                  array_type == DBUS_TYPE_STRING) {
                int len = 0;

                do {
                  ++len;
                } while (dbus_message_iter_next(&array_value));
                dbus_message_iter_recurse(&prop_value, &array_value);
                *ret_length = len;
                char** temp = (char**)malloc(sizeof(char *) * len);

                len = 0;
                do {
                  dbus_message_iter_get_basic(&array_value, &temp[len]);
                  LOG("Array Item %d: %s", len, temp[len]);

                  ++len;
                } while(dbus_message_iter_next(&array_value));

                ret_property_value->array_val = temp;
              }
              break;

            default:
              LOG("Unknown type = %d\n", property_type);
              break;
          }
        }
      }
    }
  }
}


void
BluetoothDevice::UpdateProperties()
{
  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args(mObjectPath,
                         DBUS_DEVICE_IFACE, "GetProperties",
                         DBUS_TYPE_INVALID);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      LOG("Error!\n");
    } else
      LOG("DBus reply is NULL in function %s\n", __FUNCTION__);
    return ;
  }

  DBusMessageIter iter, dict_entry, dict;
  int prop_index, array_length;
  property_value prop_value;

  if (dbus_message_iter_init(reply, &iter)) {
    int type = dbus_message_iter_get_arg_type(&iter);
    if (type != DBUS_TYPE_ARRAY) {
      LOG("Type = %d, not DUBS_TYPE_ARRAY\n", type);
      return;
    }

    dbus_message_iter_recurse(&iter, &dict);

    do {
      type = dbus_message_iter_get_arg_type(&dict);
      if (type != DBUS_TYPE_DICT_ENTRY) {
        LOG("Type = %d, not DBUS_TYPE_DICT_ENTRY\n", type);
        return;
      }

      dbus_message_iter_recurse(&dict, &dict_entry);
      get_property(dict_entry, remote_device_properties, &prop_index, &prop_value, &array_length);

      switch (prop_index)
      {
        case BT_DEVICE_ADDRESS:
          mAddress = NS_ConvertASCIItoUTF16(prop_value.str_val);
          break;
        case BT_DEVICE_NAME:
          mName = NS_ConvertASCIItoUTF16(prop_value.str_val);
          break;        
        case BT_DEVICE_CLASS:
          mClass = prop_value.int_val;
          break;        
        case BT_DEVICE_PAIRED:
          mPaired = (prop_value.int_val == 0) ? false : true;
          break;
        case BT_DEVICE_CONNECTED:
          mConnected = (prop_value.int_val == 0) ? false : true;
          break;
        case BT_DEVICE_TRUSTED:
          mTrusted = (prop_value.int_val == 0) ? false : true;
          break;        
        case BT_DEVICE_ADAPTER:
          mAdapter = NS_ConvertASCIItoUTF16(prop_value.str_val);
          break;
      }
    } while (dbus_message_iter_next(&dict));
  } else {
    LOG("Return value is wrong!\n");
  }

  dbus_message_unref(reply);
}

bool
BluetoothDevice::SetProperty(char* propertyName, int type, void* value)
{
  DBusMessage *reply, *msg;
  DBusMessageIter iter;
  DBusError err;

  /* Initialization */
  dbus_error_init(&err);

  /* Compose the command */
  msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, mObjectPath,
                                     DBUS_DEVICE_IFACE, "SetProperty");

  if (msg == NULL) {
    LOG("SetProperty : Error on creating new method call msg");
    return false;
  }

  dbus_message_append_args(msg, DBUS_TYPE_STRING, &propertyName, DBUS_TYPE_INVALID);
  dbus_message_iter_init_append(msg, &iter);
  append_variant(&iter, type, value);

  /* Send the command. */
  reply = dbus_connection_send_with_reply_and_block(mConnection, msg, -1, &err);
  dbus_message_unref(msg);

  if (!reply || dbus_error_is_set(&err)) {
    LOG("SetProperty : Send SetProperty Command error");
    return false;
  }

  return true;
}

void
asyncDiscoverServicesCallback(DBusMessage *msg, void *data, void* n)
{
  // TODO(Eric)
  LOG("Discover callback");
}

nsresult
BluetoothDevice::DiscoverServices(const nsAString& aPattern, 
                                  jsval* aServices)
{
  const char *asciiPattern = NS_LossyConvertUTF16toASCII(aPattern).get();

  char *backupObjectPath = (char *)calloc(strlen(mObjectPath), sizeof(char));
  strcpy(backupObjectPath, mObjectPath);

  LOG("... Pattern = %s, strlen = %d", asciiPattern, strlen(asciiPattern));

  bool ret = dbus_func_args_async(-1,
                                  asyncDiscoverServicesCallback,
                                  (void*)backupObjectPath,
                                  mObjectPath,
                                  DBUS_DEVICE_IFACE,
                                  "DiscoverServices",
                                  DBUS_TYPE_STRING, &asciiPattern,
                                  DBUS_TYPE_INVALID);

  return ret ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
BluetoothDevice::CancelDiscovery()
{
  DBusMessage *reply = dbus_func_args(mObjectPath,
                                      DBUS_DEVICE_IFACE, "CancelDiscovery",
                                      DBUS_TYPE_INVALID);

  return reply ? NS_OK : NS_ERROR_FAILURE;
}

// Lookup the server channel of target uuid
NS_IMETHODIMP
BluetoothDevice::QueryServerChannel(const nsAString& aServiceUuidStr, PRInt32* aChannel)
{
  // 0x0004 represents ProtocolDescriptorList. For more information, 
  // see https://www.bluetooth.org/Technical/AssignedNumbers/service_discovery.htm
  int attributeId = 0x0004;

  const char* asciiServiceUuidStr = ToNewCString(aServiceUuidStr);

  DBusMessage *reply = dbus_func_args(mObjectPath,
                                      DBUS_DEVICE_IFACE, "GetServiceAttributeValue",
                                      DBUS_TYPE_STRING, &asciiServiceUuidStr,
                                      DBUS_TYPE_UINT16, &attributeId,
                                      DBUS_TYPE_INVALID);

  *aChannel = -1;

  if (reply) {
    *aChannel = dbus_returns_int32(reply);
  }

  LOG("Handsfree: RFCOMM Server channel [%d]", *aChannel);

  return NS_OK;
}

