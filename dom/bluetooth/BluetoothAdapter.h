/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothadapter_h__
#define mozilla_dom_bluetooth_bluetoothadapter_h__

#include "nsDOMEventTargetHelper.h"
#include "nsIDOMBluetoothAdapter.h"
#include "nsIDOMDOMRequest.h"
#include "BluetoothCommon.h"
#include "BluetoothDevice.h"
#include "mozilla/ipc/DBusEventHandler.h"
#include "mozilla/ipc/RawDBusConnection.h"

class DBusMessage;
class nsIEventTarget;
class nsIDOMBluetoothDevice;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothAdapter : public nsDOMEventTargetHelper
                       , public nsIDOMBluetoothAdapter
                       , public mozilla::ipc::DBusEventHandler
                       , public mozilla::ipc::RawDBusConnection
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLUETOOTHADAPTER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothAdapter,
                                           nsDOMEventTargetHelper)

  BluetoothAdapter(nsPIDOMWindow*);

  inline void SetEnabledInternal(bool aEnabled) {mEnabled = aEnabled;}
  void Pair(const char* address, int timeout);

  nsresult FireDeviceFound(nsIDOMBluetoothDevice* aDevice);
  nsresult FirePropertyChanged(const char* aPropertyName);
  nsresult FireDeviceConnected(nsIDOMBluetoothDevice* aDevice);
  nsresult FireDeviceDisconnected(const char* aDeviceAddress);
  virtual nsresult HandleEvent(DBusMessage* msg);
  nsresult SetupBluetoothAgents(void);
  nsresult SetupBluetooth(void);

protected:
  nsresult BluezCreateDevice(const char* address);
  nsresult BluezCancelDeviceCreation(const char* address);
  void GetProperties();
  bool SetProperty(char* propertyName, int type, void* value);
  void GetAdapterPath();
  const char* GetObjectPathFromAddress(const char* aAddress);
  const char* GetAddressFromObjectPath(const char* aObjectPath);
  bool IsPaired(const char* objectPath);
  int QueryServerChannelInternal(const char* aObjectPath);
  nsresult RunAdapterFunction(const char* function_name);
  nsresult BluezRegisterAgent(const char * agent_path, const char * capabilities);
  int AddServiceRecord(const char* serviceName, 
                       unsigned long long uuidMsb, unsigned long long uuidLsb,
                       int channel);
  bool mPower;
  bool mDiscoverable;
  PRUint32 mClass;
  nsString mAddress;
  nsString mName;
  bool mPairable;
  PRUint32 mPairableTimeout;
  PRUint32 mDiscoverableTimeout;
  bool mDiscovering;
  const char* mAdapterPath;
  bool mEnabled;
  int mChannel;
  nsTArray<nsString> mDevices;
  nsTArray<nsString> mUuids;
  BluetoothSocket* mSocket;

  NS_DECL_EVENT_HANDLER(enabled)
  NS_DECL_EVENT_HANDLER(propertychanged)
  NS_DECL_EVENT_HANDLER(devicefound)
  NS_DECL_EVENT_HANDLER(devicedisappeared)
  NS_DECL_EVENT_HANDLER(devicecreated)
  NS_DECL_EVENT_HANDLER(deviceremoved)
  NS_DECL_EVENT_HANDLER(deviceconnected)
  NS_DECL_EVENT_HANDLER(devicedisconnected)
private:
  nsCOMPtr<nsIEventTarget> mToggleBtThread;
};

END_BLUETOOTH_NAMESPACE

#endif
