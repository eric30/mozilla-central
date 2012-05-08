/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
   * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothscomanager_h__
#define mozilla_dom_bluetooth_bluetoothscomanager_h__

#include "BluetoothCommon.h"
#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothScoManager {
public:
  static BluetoothScoManager* GetManager();
  bool Connect(const char* address);
  void Disconnect();
  bool Listen();
  bool IsConnected();

private:
  BluetoothScoManager();
  static void* AcceptInternal(void* ptr);
  BluetoothSocket* mSocket;
  pthread_t mAcceptThread;
  static bool sConnected;
};

END_BLUETOOTH_NAMESPACE

#endif
