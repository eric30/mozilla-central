/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothoppmanager_h__
#define mozilla_dom_bluetooth_bluetoothoppmanager_h__

#include "BluetoothObexListener.h"
#include "BluetoothCommon.h"
#include "ObexBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class ObexServer;

class BluetoothOppManager : public ObexListener
{
public:
  static const int DEFAULT_OPP_CHANNEL = 10;

  BluetoothOppManager();
  ~BluetoothOppManager();

  void Start();
  void onConnect();
  void onDisconnect();
  void onGet();
  void onPut();
  void onSetPath();

private:
  ObexServer* mServer;
};

END_BLUETOOTH_NAMESPACE

#endif
