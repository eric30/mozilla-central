/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdevice_h__
#define mozilla_dom_bluetooth_bluetoothdevice_h__

#include "BluetoothCommon.h"
#include "nsIDOMBluetoothDevice.h"
#include "nsString.h"
#include "mozilla/ipc/DBusEventHandler.h"
#include "mozilla/ipc/RawDBusConnection.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;
class BluetoothScoSocket;

class BluetoothDevice : public nsIDOMBluetoothDevice
                      , public mozilla::ipc::DBusEventHandler
                      , public mozilla::ipc::RawDBusConnection
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLUETOOTHDEVICE

  BluetoothDevice(const char* aAddress, const char* aName, const char* aObjectPath);
  virtual nsresult HandleEvent(DBusMessage* msg);

protected:
  char* mObjectPath;
  nsString mAdapter;
  nsString mAddress;
  PRUint32 mClass;
  bool mConnected;
  bool mLegacyPairing;
  nsString mName;
  bool mPaired;
  nsTArray<nsString> mUuids;

  bool mTrusted;
  nsString mAlias;

  bool SetProperty(char* propertyName, int type, void* value);
  void UpdateProperties();

private:
  BluetoothSocket* mSocket;
  BluetoothScoSocket* mScoSocket;
};

END_BLUETOOTH_NAMESPACE

#endif
