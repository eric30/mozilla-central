/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothScoManager.h"
#include "BluetoothSocket.h"

#include "AudioManager.h"
#include "nsServiceManagerUtils.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

static BluetoothScoManager* sInstance = NULL;

BluetoothScoManager::BluetoothScoManager() : mSocket(NULL)
                                           , mServerSocket(NULL)
{
  mAudioManager = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
}

BluetoothScoManager*
BluetoothScoManager::GetManager()
{
  if (sInstance == NULL)
  {
    sInstance = new BluetoothScoManager();
  }

  return sInstance;
}

bool 
BluetoothScoManager::Connect(const char* aAddress)
{
  if (this->IsConnected()) {
    LOG("SCO connection has been already existed.");
    return false;
  }

  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO, -1, true, false, NULL);

  LOG("Connect SCO : %s", aAddress);

  int ret = mSocket->Connect(aAddress, -1);
  if (ret) {
    LOG("Connect failed - SCO Socket");

    delete mSocket;
    mSocket = NULL;

    return false;
  }

  LOG("Connect successfully - SCO Socket");

  // TODO(Eric)
  // I'm not sure if using SetForceForUse to route audio is right.
  //mAudioManager->SetForceForUse(nsIAudioManager::USE_COMMUNICATION, 
  //                              nsIAudioManager::FORCE_BT_SCO);
  mozilla::dom::gonk::AudioManager::SetAudioRoute(3);

  return true;
}

void
BluetoothScoManager::Disconnect()
{
  if (!this->IsConnected()) {
    LOG("No SCO connected.");
    return;
  }

  LOG("Disconnect SCO Socket in BluetoothScoManager");

  mSocket->CloseInternal();

  delete mSocket;
  mSocket = NULL;
}

bool
BluetoothScoManager::IsConnected()
{
  return (mSocket != NULL);
}
