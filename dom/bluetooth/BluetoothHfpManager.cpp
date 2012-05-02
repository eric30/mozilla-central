#include "BluetoothHfpManager.h"
#include "BluetoothSocket.h"
#include "AudioManager.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

static BluetoothHfpManager* sInstance = NULL;
static bool sStopEventLoopFlag = false;

BluetoothHfpManager::BluetoothHfpManager() : mSocket(NULL)
                                           , mScoSocket(NULL)
                                           , mConnected(false)
                                           , mChannel(-1)
{
  // Do nothing
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
BluetoothHfpManager::ReachedMaxConnection()
{
  // Now we can only have one conenction at a time
  return mConnected;
}

void
BluetoothHfpManager::Disconnect()
{
  if (mSocket != NULL)
  {
    mSocket->Disconnect();

    delete mSocket;
    mSocket = NULL;
  }

  if (mScoSocket != NULL)
  {
    mScoSocket->Disconnect();

    delete mScoSocket;
    mScoSocket = NULL;
  }

  mConnected = false;
  sStopEventLoopFlag = true;
}

bool 
BluetoothHfpManager::Connect(int channel, const char* asciiAddress)
{
  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM);
  }

  if (mSocket->Connect(channel, asciiAddress)) {
    mConnected = true;
    pthread_create(&mEventThread, NULL, BluetoothHfpManager::MessageHandler, mSocket);

    // Connect ok, next : establish a SCO link
    if (mScoSocket == NULL || !mScoSocket->Available()) {
      mScoSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);
    }

    if (mScoSocket->Connect(-1, asciiAddress)) {
      mozilla::dom::gonk::AudioManager::SetAudioRoute(3);
    } else {
      delete mScoSocket;
      mScoSocket = NULL;
    }
  } else {
    delete mSocket;
    mSocket = NULL;

    return false;
  }

  return true;
}

void reply_ok(int fd)
{
  if (BluetoothSocket::send_line(fd, "OK") != 0) {
    LOG("Reply [OK] failed");
  }
}

void reply_error(int fd)
{
  if (BluetoothSocket::send_line(fd, "ERROR") != 0) {
    LOG("Reply [ERROR] failed");
  }
}

void reply_brsf(int fd)
{
  if (BluetoothSocket::send_line(fd, "+BRSF: 23") != 0) {
    LOG("Reply +BRSF failed");
  }
}

void reply_cind_current_status(int fd)
{
  const char* str = "+CIND: 1,0,0,0,3,0,3";

  if (BluetoothSocket::send_line(fd, str) != 0) {
    LOG("Reply +CIND failed");
  }
}

void reply_cind_range(int fd)
{
  const char* str = "+CIND: (\"service\",(0-1)),(\"call\",(0-1)),(\"callsetup\",(0-3)), \
                     (\"callheld\",(0-2)),(\"signal\",(0-5)),(\"roam\",(0-1)), \
                     (\"battchg\",(0-5))";

  if (BluetoothSocket::send_line(fd, str) != 0) {
    LOG("Reply +CIND=? failed");
  }
}

void reply_cmer(int fd, bool enableIndicator)
{
  const char* str = enableIndicator ? "+CMER: 3,0,0,1" : "+CMER: 3,0,0,0";

  if (BluetoothSocket::send_line(fd, str) != 0) {
    LOG("Reply +CMER= failed");
  }
}

void reply_chld_range(int fd)
{
  const char* str = "+CHLD: (0,1,2,3)";

  if (BluetoothSocket::send_line(fd, str) != 0) {
    LOG("Reply +CHLD=? failed");
  }
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

    const char *ret = BluetoothSocket::get_line(socket->mFd,
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
      } else {
        LOG("Not handled.");
        reply_ok(socket->mFd);
      }
    }
  }  

  return NULL;
}
