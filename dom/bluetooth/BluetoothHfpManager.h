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
    static const int DEFAULT_HFP_CHANNEL = 3;
    static const int DEFAULT_HSP_CHANNEL = 4;
    static const int MAX_SLC = 1;

    static BluetoothHfpManager* GetManager();
    BluetoothSocket* Connect(const char* asciiAddress, int channel);
    void Disconnect();
    bool IsConnected();
    bool WaitForConnect();
    void StopWaiting();

    pthread_t mEventThread;
    pthread_t mAcceptThread;
  private:
    BluetoothHfpManager();
    static void* MessageHandler(void* ptr);
    static void* AcceptInternal(void* ptr);
    BluetoothSocket* mSocket;
    BluetoothSocket* mServerSocket;
    int mFileDescriptor;
    bool mConnected;
    int mChannel;
    char* mAddress;
};

END_BLUETOOTH_NAMESPACE

#endif
