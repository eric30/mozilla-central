/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDevice.h"
#include "BluetoothCommon.h"
#include "BluetoothSocket.h"

#include "dbus/dbus.h"
#include "nsDOMClassInfo.h"
#include "nsString.h"

#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"

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

BluetoothDevice::BluetoothDevice(const char* aAddress, const char* aObjectPath) : mSocket(NULL)
{
  mAddress = NS_ConvertASCIItoUTF16(aAddress);

  mObjectPath = (char *)calloc(strlen(aObjectPath), sizeof(char));
  strcpy(mObjectPath, aObjectPath);
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

  if (!SetProperty("Pairable", DBUS_TYPE_BOOLEAN, (void*)&value)) {
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

  if (!SetProperty("Name", DBUS_TYPE_STRING, (void*)&asciiAlias)) {
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
BluetoothDevice::Connect(PRInt32 channel)
{
  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket();
  }

  const char* asciiAddress = NS_LossyConvertUTF16toASCII(mAddress).get();
  mSocket->Connect(channel, asciiAddress);

  return NS_OK;
}

nsresult
BluetoothDevice::Disconnect()
{
  if (mSocket != NULL) {
    mSocket->Disconnect();
  }

  return NS_OK;
}

nsresult
BluetoothDevice::GetProperties(jsval* aProperties)
{
  return NS_OK;
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

