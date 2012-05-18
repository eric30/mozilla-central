/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDevice.h"
#include "nsDOMClassInfo.h"

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
