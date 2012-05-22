/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothscomanager_h__
#define mozilla_dom_bluetooth_bluetoothscomanager_h__

#include "BluetoothCommon.h"
#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothScoManager
{
  public:
    static BluetoothScoManager* GetManager();
    bool Connect(const char* asciiAddress);
    void Disconnect();
    bool IsConnected();
   // bool WaitForConnect();
  //  void StopWaiting();
    pthread_t mAcceptThread;

  private:
    BluetoothScoManager();

    BluetoothSocket* mSocket;
    BluetoothSocket* mServerSocket;
};

END_BLUETOOTH_NAMESPACE

#endif
