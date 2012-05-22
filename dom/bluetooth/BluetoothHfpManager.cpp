/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHfpManager.h"
#include "BluetoothHfpBase.h"

#include "BluetoothDevice.h"
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

static BluetoothHfpManager* sInstance = NULL;
static bool sStopEventLoopFlag = true;
static bool sStopAcceptThreadFlag = true;
static pthread_t sDisconnectThread;
static pthread_t sCreateScoThread;

static void*
DisconnectThreadFunc(void* ptr)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  hfp->Disconnect();

  return NULL;
}

static void*
CreateScoThreadFunc(void* ptr)
{
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);

  const char* address = socket->GetRemoteDeviceAddress();
  LOG("Start to connect SCO - %s", address);

  BluetoothScoManager* sco = BluetoothScoManager::GetManager();
  sco->Connect(address);

  delete address;
  LOG("Finish connecting SCO");

  return NULL;
}

BluetoothHfpManager::BluetoothHfpManager() : mSocket(NULL)
                                           , mServerSocket(NULL)
                                           , mChannel(-1)
                                           , mAddress(NULL)
                                           , mEventThread(NULL)
{
  mAudioManager = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
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

bool
BluetoothHfpManager::WaitForConnect()
{
  // Because it is Bluetooth 'HFP' Manager to listen,
  // it's definitely listen to HFP channel. So no need
  // to pass channel argument in.

  if (mServerSocket != NULL) {
    LOG("Already connected or forget to clear mServerSocket?");
    return false;
  }

  mServerSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, -1, true, false, NULL);
  LOG("Created a new BluetoothServerSocket for listening");

  mChannel = BluetoothHfpManager::DEFAULT_HFP_CHANNEL;

  int ret = mServerSocket->BindListen(mChannel);
  if (ret != 0) {
    LOG("BindListen failed. error no is %d", ret);
    return false;
  }

  pthread_create(&(mAcceptThread), NULL, BluetoothHfpManager::AcceptInternal, mServerSocket);

  return true;
}

void 
BluetoothHfpManager::StopWaiting()
{
  sStopAcceptThreadFlag = true;

  mServerSocket->CloseInternal();

  delete mServerSocket;
  mServerSocket = NULL;
}

BluetoothSocket*
BluetoothHfpManager::Connect(const char* aAddress, int aChannel)
{
  if (aChannel <= 0) return NULL;
  if (IsConnected()) return NULL;

  BluetoothDevice* newDevice = new BluetoothDevice(aAddress);
  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, -1, true, false, newDevice);

  int ret = mSocket->Connect(aAddress, aChannel);
  if (ret) {
    LOG("Connect failed - RFCOMM Socket");

    delete newDevice;
    delete mSocket;
    mSocket = NULL;

    return NULL;
  } 

  LOG("Connect successfully - RFCOMM Socket");
  pthread_create(&mEventThread, NULL, BluetoothHfpManager::MessageHandler, mSocket);

  return mSocket;
}

void 
BluetoothHfpManager::Disconnect() 
{
  LOG("Disconnect RFCOMM Socket in BluetoothHfpManager");

  if (this->IsConnected()) {
    // Stop event loop first
    sStopEventLoopFlag = true;
    pthread_join(mEventThread, NULL);
    mEventThread = NULL;

    mSocket->CloseInternal();
    mSocket = NULL;

    BluetoothScoManager* sco = BluetoothScoManager::GetManager();
    sco->Disconnect();
  }
}

bool 
BluetoothHfpManager::IsConnected() 
{
  return (mEventThread != NULL);
}

void*
BluetoothHfpManager::AcceptInternal(void* ptr)
{
  BluetoothSocket* serverSocket = static_cast<BluetoothSocket*>(ptr);
  sStopAcceptThreadFlag = false;

  while (!sStopAcceptThreadFlag) {
    BluetoothSocket* newSocket = serverSocket->Accept();

    if (newSocket == NULL) {
      LOG("Accepted failed.");
      continue;
    }

    BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();

    if (hfp->mSocket != NULL || hfp->IsConnected()) {
      LOG("A socket has been accepted, however there is no available resource.");
      newSocket->CloseInternal();

      delete newSocket;
      continue;
    }

    hfp->mSocket = newSocket;
    pthread_create(&hfp->mEventThread, NULL, BluetoothHfpManager::MessageHandler, hfp->mSocket);
  }

  return NULL;
}

void*
BluetoothHfpManager::MessageHandler(void* ptr)
{
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  int err;
  sStopEventLoopFlag = false;

  while (!sStopEventLoopFlag) {
    int timeout = 500; //0.5 sec
    char buf[256];
    const char *ret = get_line(socket->mFd,
                               buf, sizeof(buf),
                               timeout,
                               &err);

    if (ret == NULL) {
      if (err != 0) {
        LOG("Read error %d in %s. Start disconnect routine.", err, __FUNCTION__);
        pthread_create(&sDisconnectThread, NULL, DisconnectThreadFunc, NULL);
        break;
      } else {
        LOG("HFP: Read Nothing");
      }
    } else {
      if (!strncmp(ret, "AT+BRSF=", 8)) {
        hfp->mHfBrsf = ret[8] - '0';

        if (strlen(ret) > 9) {
          hfp->mHfBrsf = hfp->mHfBrsf * 10 + (ret[9] - '0');
        }

        reply_brsf(socket->mFd, BluetoothHfpManager::BRSF);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CIND=?", 9)) {
        reply_cind_range(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CIND", 7)) {
        reply_cind_current_status(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CMER=", 8)) {
        reply_ok(socket->mFd);

        // Create SCO once SLC has been established.
        // According to HFP spec figure 4.1 and section 4.11, we said SLC 
        // is 'established' after AG sent ok for HF's AT+CMER, and SCO connection 
        // process shall start after that.
        pthread_create(&sCreateScoThread, NULL, CreateScoThreadFunc, socket);
      } else if (!strncmp(ret, "AT+CHLD=?", 9)) {
        reply_chld_range(socket->mFd);
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+CHLD=", 9)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+VGS=", 7)) {
        int newVgs = ret[7] - '0';

        if (strlen(ret) > 8) {
          newVgs = newVgs * 10 + (ret[8] - '0');
        }

        // Because the range of VGS is [0, 15], and value of
        // MasterVolume is [0, 1], so normalize it.
        hfp->mAudioManager->SetMasterVolume((float)newVgs / 15.0f);

        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+VGM=", 7)) {
        // Currently, we only provide two options for mic volume 
        // settings: mute and unmute.
        int newVgm = ret[7] - '0';

        if (strlen(ret) > 8) {
          newVgm = newVgm * 10 + (ret[8] - '0');
        }

        hfp->mAudioManager->SetMicrophoneMuted(newVgm == 0 ? true : false);

        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "ATA", 3)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+BLDN", 7)) {
        reply_ok(socket->mFd);
      } else if (!strncmp(ret, "AT+BVRA", 7)) {
        // Currently, we do not support voice recognition
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

