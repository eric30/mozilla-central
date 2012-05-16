/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothadapter_h__
#define mozilla_dom_bluetooth_bluetoothadapter_h__

#include "BluetoothCommon.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMBluetoothAdapter.h"
#include "nsIDOMDOMRequest.h"

class nsIEventTarget;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothEventHandler;

class BluetoothAdapter : public nsDOMEventTargetHelper
                       , public nsIDOMBluetoothAdapter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLUETOOTHADAPTER

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothAdapter,
                                           nsDOMEventTargetHelper)

  BluetoothAdapter(nsPIDOMWindow*);
  void Setup();
  void TearDown();

  inline void SetEnabledInternal(bool aEnabled) {mEnabled = aEnabled;}

  void onDeviceFoundNative(const char* aDeviceAddress);
  void onDeviceCreatedNative(const char* aDeviceObjectPath);

protected:
  bool mEnabled;
  bool mDiscoverable;
  bool mDiscovering;
  nsString mAddress;
  nsString mName;
  PRUint32 mDiscoverableTimeout;
  PRUint32 mClass;

  NS_DECL_EVENT_HANDLER(enabled)

private:
  nsCOMPtr<nsIEventTarget> mToggleBtThread;
  BluetoothEventHandler* mHandler;
};

END_BLUETOOTH_NAMESPACE

#endif
