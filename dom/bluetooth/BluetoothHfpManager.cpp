/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHfpManager.h"
#include "BluetoothHfpBase.h"

#include "BluetoothCallManager.h"
#include "BluetoothDevice.h"
#include "BluetoothScoManager.h"
#include "BluetoothSocket.h"

#include "AudioManager.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

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
static pthread_t sDisconnectScoThread;

static void*
DisconnectThreadFunc(void* ptr)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  hfp->Disconnect();

  return NULL;
}

static void*
DisconnectScoThreadFunc(void* ptr)
{
  BluetoothScoManager* sco = BluetoothScoManager::GetManager();
  sco->Disconnect();

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
                                           , mState(0)
{
  mAudioManager = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  mCallManager = new BluetoothCallManager();
}

BluetoothHfpManager::~BluetoothHfpManager()
{
  delete mCallManager;
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
  if (this->IsConnected()) {
    BluetoothScoManager* sco = BluetoothScoManager::GetManager();
    sco->Disconnect();

    sStopEventLoopFlag = true;
    pthread_join(mEventThread, NULL);
    mEventThread = NULL;    
    
    LOG("Disconnect RFCOMM Socket in BluetoothHfpManager");

    mSocket->CloseInternal();
    mSocket = NULL;
  }
}

bool 
BluetoothHfpManager::IsConnected() 
{
  return (mEventThread != NULL);
}

bool
BluetoothHfpManager::AudioOn()
{
  if (this->mState != 2) {
    LOG("SLC must be established before SCO.");
    return false;
  }

  pthread_create(&sCreateScoThread, NULL, CreateScoThreadFunc, this->mSocket);
  return true;
}

void
BluetoothHfpManager::AudioOff()
{
  pthread_create(&sDisconnectScoThread, NULL, DisconnectScoThreadFunc, NULL);
}

void
BluetoothHfpManager::AtCommandParser(const char* aCommandStr)
{
  int fd = this->mSocket->mFd;

  LOG("aCommandStr: %s", aCommandStr);

  if (!strncmp(aCommandStr, "AT+BRSF=", 8)) {
    this->mHfBrsf = aCommandStr[8] - '0';

    if (strlen(aCommandStr) > 9) {
      this->mHfBrsf = this->mHfBrsf * 10 + (aCommandStr[9] - '0');
    }

    reply_brsf(fd, BluetoothHfpManager::BRSF);
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+CIND=?", 9)) {
    reply_cind_range(fd);
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+CIND", 7)) {
    reply_cind_current_status(fd);
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+CMER=", 8)) {
    reply_ok(fd);

    // Create SCO once SLC has been established.
    // According to HFP spec figure 4.1 and section 4.11, we said SLC 
    // is 'established' after AG sent ok for HF's AT+CMER, and SCO connection 
    // process shall start after that.
    this->mState = 2;
    this->AudioOn();
  } else if (!strncmp(aCommandStr, "AT+CHLD=?", 9)) {
    reply_chld_range(fd);
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+CHLD=", 9)) {
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+VGS=", 7)) {
    int newVgs = aCommandStr[7] - '0';

    if (strlen(aCommandStr) > 8) {
      newVgs = newVgs * 10 + (aCommandStr[8] - '0');
    }

    // Because the range of VGS is [0, 15], and value of
    // MasterVolume is [0, 1], so normalize it.
    this->mAudioManager->SetMasterVolume((float)newVgs / 15.0f);

    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+VGM=", 7)) {
    // Currently, we only provide two options for mic volume 
    // settings: mute and unmute.
    int newVgm = aCommandStr[7] - '0';

    if (strlen(aCommandStr) > 8) {
      newVgm = newVgm * 10 + (aCommandStr[8] - '0');
    }

    this->mAudioManager->SetMicrophoneMuted(newVgm == 0 ? true : false);

    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "ATA", 3)) {
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+BLDN", 7)) {
    //BluetoothCallManager::HangUp();
    mCallManager->Answer();
    reply_ok(fd);
  } else if (!strncmp(aCommandStr, "AT+BVRA", 7)) {
    // Currently, we do not support voice recognition
    reply_error(fd);
  } else if (!strncmp(aCommandStr, "OK", 2)) {
    // Do nothing
    LOG("Got an OK");
  } else {
    LOG("Not handled.");
    reply_ok(fd);
  }
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

class ParseATCommandTask : public nsRunnable
{
public:
  ParseATCommandTask(int aFd, const char* aCommandStr) : mFd(aFd)
                                                       , mCommandStr(aCommandStr)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
    hfp->AtCommandParser(mCommandStr);

    delete mCommandStr;

    return NS_OK;
  }

private:
  int mFd;
  const char* mCommandStr;
};

void*
BluetoothHfpManager::MessageHandler(void* ptr)
{
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  int err;
  sStopEventLoopFlag = false;
  hfp->mState = 1;

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
      char* newStr = new char[strlen(ret) + 1];
      newStr[strlen(ret)] = '\0';
      strcpy(newStr, ret);

      nsCOMPtr<nsIRunnable> commandParserTask = new ParseATCommandTask(socket->mFd, newStr);

      if (NS_FAILED(NS_DispatchToMainThread(commandParserTask))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }
    }
  }

  hfp->mState = 0;

  return NULL;
}

