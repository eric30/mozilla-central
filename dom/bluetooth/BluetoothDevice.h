/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdevice_h__
#define mozilla_dom_bluetooth_bluetoothdevice_h__

#include "BluetoothCommon.h"
#include "nsIDOMBluetoothDevice.h"
#include "nsString.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothDevice : public nsIDOMBluetoothDevice
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLUETOOTHDEVICE

  BluetoothDevice(const char* aAddress);

  const char* GetAddressInternal();

private:
  nsString mAddress;

  // xxx Temp
  BluetoothSocket* mSocket;
};

END_BLUETOOTH_NAMESPACE

#endif
