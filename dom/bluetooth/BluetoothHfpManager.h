/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanager_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothCallManager.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIAudioManager.h"
#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;

class BluetoothHfpManager
{
  public:
    static const int DEFAULT_HFP_CHANNEL = 3;
    static const int DEFAULT_HSP_CHANNEL = 4;
    static const int MAX_SLC = 1;

    /*
      According to HFP 1.5 spec 4.33.3 Bluetooth Defined AT Capabilities,
      the definition of the bit mask of BRSF is listed as below:
      Bit  Feature
      0    EC and/or NR function
      1    Call waiting and 3-way calling
      2    CLI presentation capability
      3    Voice recognition activation
      4    Remote volume control
      5    Enhanced call status
      6    Enhanced call control
      7-31 Reserved for future definition
    */
    static const int BRSF = 16;

    static BluetoothHfpManager* GetManager();
    BluetoothSocket* Connect(const char* asciiAddress, int channel);
    void Disconnect();
    bool IsConnected();
    bool WaitForConnect();
    void StopWaiting();
    bool AudioOn();
    void AudioOff();
    void AtCommandParser(const char* aCommandStr);

    pthread_t mEventThread;
    pthread_t mAcceptThread;

  private:
    BluetoothHfpManager();
    ~BluetoothHfpManager();
    static void* MessageHandler(void* ptr);
    static void* AcceptInternal(void* ptr);
    nsCOMPtr<nsIAudioManager> mAudioManager;
    BluetoothCallManager* mCallManager;
    BluetoothSocket* mSocket;
    BluetoothSocket* mServerSocket;
    int mFileDescriptor;
    int mChannel;
    char* mAddress;
    int mHfBrsf;
    int mState;
};

END_BLUETOOTH_NAMESPACE

#endif
