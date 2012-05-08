#include "BluetoothHfpManager.h"
#include "BluetoothScoManager.h"
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

bool 
BluetoothHfpManager::ReachedMaxConnection()
{
  // Now we can only have one conenction at a time
  return mConnected;
}

void
BluetoothHfpManager::Disconnect()
{
  void* ret;

  LOG("Disconnect");

  if (mSocket != NULL)
  {
    mSocket->Disconnect();

    LOG("Threadjoin!");

    sStopEventLoopFlag = true;
    pthread_join(mEventThread, &ret);

    delete mSocket;
    mSocket = NULL;
  }

  Listen(mChannel);

  BluetoothScoManager* scoManager = BluetoothScoManager::GetManager(); 
  scoManager->Disconnect();

  mConnected = false;
}

bool 
BluetoothHfpManager::Connect(int channel, const char* asciiAddress)
{
  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM);
  }

  if (mSocket->Connect(channel, asciiAddress)) {
    LOG("Connect successfully");
    mConnected = true;
    pthread_create(&mEventThread, NULL, BluetoothHfpManager::MessageHandler, mSocket);

    // Connect ok, next : establish a SCO link
    BluetoothScoManager* scoManager = BluetoothScoManager::GetManager();

    if (!scoManager->IsConnected()) {
      scoManager->Connect(asciiAddress);
    }
  } else {
    LOG("Connect failed");
    delete mSocket;
    mSocket = NULL;

    return false;
  }

  return true;
}

bool
BluetoothHfpManager::Listen(int channel)
{
  if (channel > 0) {
    if (mSocket == NULL || !mSocket->Available()) {
      mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM);
      LOG("Create a new BluetoothSocket");
    }

    LOG("Listening to channel %d", channel);

    mChannel = channel;
    mSocket->Listen(channel);
    pthread_create(&(mAcceptThread), NULL, BluetoothHfpManager::AcceptInternal, mSocket);

    return true;
  } else {
    return false;
  }
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
BluetoothHfpManager::AcceptInternal(void* ptr)
{
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);
  int newFd = socket->Accept();

  if (newFd > 0) {
    BluetoothHfpManager* manager = BluetoothHfpManager::GetManager();
    pthread_create(&manager->mEventThread, NULL, BluetoothHfpManager::MessageHandler, ptr);

    // Connect ok, next : establish a SCO link
    BluetoothScoManager* scoManager = BluetoothScoManager::GetManager();

    if (!scoManager->IsConnected()) {
      const char* address = socket->GetAddress();
      LOG("[ERIC] SCO address : %s", address);
      scoManager->Connect(address);
    }
  }

  return NULL;
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
