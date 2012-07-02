/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothobexmanager_h__
#define mozilla_dom_bluetooth_bluetoothobexmanager_h__

#include "BluetoothCommon.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIAudioManager.h"
#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothObexManager
{
  public:
    static const int DEFAULT_OPP_CHANNEL = 10;
    static const int DEFAULT_FTP_CHANNEL = 11;

    static BluetoothObexManager* GetManager();
    bool WaitForConnect();    
    bool IsConnected();
    void StopWaiting();

    BluetoothSocket* Connect(const char* asciiAddress, int channel);
    void Disconnect();

    pthread_t mEventThread;
    pthread_t mAcceptThread;

  private:
    BluetoothObexManager();
    ~BluetoothObexManager();
    static void* MessageHandler(void* ptr);
    static void* AcceptInternal(void* ptr);
    
    BluetoothSocket* mSocket;
    BluetoothSocket* mServerSocket;
    int mChannel;
    char* mAddress;
};

END_BLUETOOTH_NAMESPACE

#endif
