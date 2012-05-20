/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDevice.h"
#include "nsDOMClassInfo.h"

// xxx Temp
#include "BluetoothSocket.h"
#include "BluetoothService.h"

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
BluetoothDevice::Connect(const nsAString& aServiceUuid)
{
  const char* serviceUuid = NS_LossyConvertUTF16toASCII(aServiceUuid).get();
  // 1. Check if it's paired (TODO)
  // 2. Connect by BluetoothSocket
  const char* address = NS_LossyConvertUTF16toASCII(mAddress).get();
  const char* objectPath = GetObjectPathFromAddress(address);
  int channel = GetDeviceServiceChannelInternal(objectPath, serviceUuid, 0x0004);

  LOG("Channel of hfp service : %d", channel);
 
  // xxx Temp
  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, this);
  mSocket->Init(true, false);
  mSocket->Connect(address, channel);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothDevice::Close()
{
  mSocket->Close();

  return NS_OK;
}
