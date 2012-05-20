/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHfpManager.h"
#include "BluetoothHfpBase.h"
#include "BluetoothSocket.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

static BluetoothHfpManager* sInstance = NULL;
static bool sStopEventLoopFlag = true;

BluetoothHfpManager::BluetoothHfpManager() : mSocket(NULL)
                                           , mConnected(false)
                                           , mChannel(-1)
                                           , mAddress(NULL)
{
}

BluetoothHfpManager*
BluetoothHfpManager::GetManager()
{
  if (sInstance == NULL)
  {
    sInstance = new BluetoothHfpManager();
  }

  return sInstance;
}

BluetoothSocket*
BluetoothHfpManager::Connect(const char* aAddress, int aChannel)
{
  if (aChannel <= 0) return NULL;

  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, NULL);
  mSocket->Init(true, false);

  int ret = mSocket->Connect(aAddress, aChannel);
  if (ret) {
    LOG("Connect failed");
    delete mSocket;
    mSocket = NULL;

    return NULL;
  } 

  LOG("Connect successfully");
  mConnected = true;
  pthread_create(&mEventThread, NULL, BluetoothHfpManager::MessageHandler, mSocket);

  return mSocket;
}

void 
BluetoothHfpManager::Disconnect() 
{
  if (mEventThread != NULL) {
    sStopEventLoopFlag = true;
    pthread_join(mEventThread, NULL);
    mEventThread = NULL;
  }

  if (mSocket != NULL && mSocket->mFd > 0) {
    mSocket->CloseInternal();
  }
}

bool 
BluetoothHfpManager::IsConnected() 
{
  return (mEventThread != NULL);
}

void*
BluetoothHfpManager::MessageHandler(void* ptr)
{
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);
  int err;
  sStopEventLoopFlag = false;

  while (!sStopEventLoopFlag)
  {
    int timeout = 500; //0.5 sec
    char buf[256];
    const char *ret = get_line(socket->mFd,
        buf, sizeof(buf),
        timeout,
        &err);

    if (ret == NULL) {
      LOG("HFP: Read Nothing");
    } else {
      LOG("HFP: Received:%s", ret);

      if (!strncmp(ret, "AT+BRSF=", 8)) {
        reply_brsf(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CIND=?", 9)) {
        reply_cind_range(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CIND", 7)) {
        reply_cind_current_status(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CMER=", 8)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CHLD=?", 9)) {
        reply_chld_range(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CHLD=", 9)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+VGS=", 7)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+VGM=", 7)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "ATA", 3)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+BLDN", 7)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+BVRA", 7)) {
        reply_error(socket->mFd);
      } else if (!strncmp(ret, "OK", 2)) {
        // Do nothing
        LOG("Got an OK");
      } else {
        LOG("Not handled.");
        reply_ok(socket->mFd);
      }
    }
  }

  return NULL;
}

