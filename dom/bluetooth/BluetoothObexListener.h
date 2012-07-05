/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothobexlistener_h__
#define mozilla_dom_bluetooth_bluetoothobexlistener_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class ObexListener
{
public:
  virtual void onConnect() = 0;
  virtual void onSetPath() = 0;
  virtual void onPut() = 0;
  virtual void onGet() = 0;
  virtual void onDisconnect() = 0;
};

END_BLUETOOTH_NAMESPACE

#endif
