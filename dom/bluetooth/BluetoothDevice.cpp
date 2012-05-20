/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDevice.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMBluetoothSocket.h"

// xxx Temp
#include "BluetoothSocket.h"
#include "BluetoothService.h"
#include "BluetoothHfpManager.h"
#include "BluetoothServiceUuid.h"

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

BluetoothDevice::BluetoothDevice(const char* aAddress)
{
  mAddress = NS_ConvertASCIItoUTF16(aAddress);
}

NS_IMETHODIMP
BluetoothDevice::GetAddress(nsAString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothDevice::Connect(const nsAString& aServiceUuid, nsIDOMBluetoothSocket** aSocket)
{
  nsCOMPtr<nsIDOMBluetoothSocket> socket;
  const char* serviceUuid = NS_LossyConvertUTF16toASCII(aServiceUuid).get();
  // 1. Check if it's paired (TODO)
  // 2. Connect by BluetoothSocket
  const char* address = NS_LossyConvertUTF16toASCII(mAddress).get();
  const char* objectPath = GetObjectPathFromAddress(address);
  int channel = GetDeviceServiceChannelInternal(objectPath, serviceUuid, 0x0004);

  if (!strcmp(serviceUuid, BluetoothServiceUuidStr::Handsfree)) {
    LOG("Channel of hfp service : %d", channel);

    BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
    socket = hfp->Connect(address, channel);
  } else {
    // TODO(Eric)
    LOG("Not implemented yet");
  }

  socket.forget(aSocket);
 
  return NS_OK;
}

NS_IMETHODIMP
BluetoothDevice::Disconnect()
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();

  if (hfp->IsConnected()) {
    hfp->Disconnect();
  }

  return NS_OK;
}
