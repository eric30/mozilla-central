/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanager_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanager_h__

#include "BluetoothCommon.h"
#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothHfpManager
{
  public:
    static BluetoothHfpManager* GetManager();
    BluetoothSocket* Connect(const char* asciiAddress, int channel);
    void Disconnect();
    bool IsConnected();
    /*
    bool Listen(int channel);
    */
    pthread_t mEventThread;

  private:
    BluetoothHfpManager();
    static void* MessageHandler(void* ptr);
    BluetoothSocket* mSocket;
    int mFileDescriptor;
    bool mConnected;
    int mChannel;
    char* mAddress;
};

END_BLUETOOTH_NAMESPACE

#endif
