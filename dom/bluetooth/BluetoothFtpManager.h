/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothftpmanager_h__
#define mozilla_dom_bluetooth_bluetoothftpmanager_h__

#include "BluetoothObexListener.h"
#include "BluetoothCommon.h"
#include "ObexBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class ObexServer;

class BluetoothFtpManager : public ObexListener
{
public:
  static const int DEFAULT_FTP_CHANNEL = 8;

  BluetoothFtpManager();
  ~BluetoothFtpManager();

  void Start();
  char onConnect();
  char onDisconnect();
  char onPut(const ObexHeaderSet& reqHeaderSet, char* response);
  char onGet(const ObexHeaderSet& reqHeaderSet, char* response);
  char onSetPath(const ObexHeaderSet& reqHeaderSet, char* response);

private:
  ObexServer* mServer;
};

END_BLUETOOTH_NAMESPACE

#endif
