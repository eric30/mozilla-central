#include "BluetoothScoManager.h"
#include "BluetoothSocket.h"
#include "AudioManager.h"
#include <unistd.h> /* usleep() */


#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

static BluetoothScoManager* sInstance = NULL;
bool BluetoothScoManager::sConnected = false;
bool sStopFlag = false;
bool sStopRouteFlag = true;

BluetoothScoManager::BluetoothScoManager() : mSocket(NULL)
                                           , mServerSocket(NULL)
{
  // Do nothing
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

void
BluetoothScoManager::Close()
{
  void* ret;

  if (mServerSocket != NULL)
  {
    // Stop accepting connection thread
    mServerSocket->Disconnect();

    sStopFlag = true;
    //pthread_join(mAcceptThread, &ret);

    delete mServerSocket;
    mServerSocket = NULL;
  }
}

void
BluetoothScoManager::Disconnect()
{
  sStopRouteFlag = true;

  if (mSocket != NULL)
  {
    mSocket->Disconnect();

    delete mSocket;
    mSocket = NULL;
  }

  BluetoothScoManager::sConnected = false;
  mozilla::dom::gonk::AudioManager::BluetoothSco = false;
  mozilla::dom::gonk::AudioManager::SetAudioRoute(0);
}

bool
BluetoothScoManager::Connect(const char* address)
{
  if (mSocket != NULL) {
    LOG("Disconnect???????");
    mSocket->Disconnect();

    delete mSocket;
    mSocket = NULL;
  }

  LOG("WTF 1");

  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);

  if (mSocket->Connect(1, address)) {
    mozilla::dom::gonk::AudioManager::SetAudioRoute(3);
    BluetoothScoManager::sConnected = true;
    mozilla::dom::gonk::AudioManager::BluetoothSco = true;

    pthread_create(&(mRouteThread), NULL,
                     BluetoothScoManager::RouteAudioInternal, NULL);

    LOG("SCO connected");
  } else {
    delete mSocket;
    mSocket = NULL;

    LOG("SCO failed");

    return false;
  }


  return true;
}

void*
BluetoothScoManager::AcceptInternal(void* ptr)
{
  BluetoothSocket* serverSocket = static_cast<BluetoothSocket*>(ptr);

  sStopFlag = false;

  while (!sStopFlag) {
    int newFd = serverSocket->Accept();

    if (newFd <= 0) {
      LOG("Accept SCO connection failed, socket fd = %d", newFd);
      break;
    }

    BluetoothScoManager::sConnected = true;
    mozilla::dom::gonk::AudioManager::BluetoothSco = true;
  }

  return NULL;
}

void*
BluetoothScoManager::RouteAudioInternal(void* ptr)
{
  sStopRouteFlag = false;

  while (!sStopRouteFlag) {
    usleep(5000);
    mozilla::dom::gonk::AudioManager::SetAudioRoute(3);
  }

  return NULL;
}


bool
BluetoothScoManager::Listen()
{
  if (mServerSocket != NULL)
    return false;

  while (true) {
    if (mServerSocket != NULL) {
      mServerSocket->Disconnect();

      delete mServerSocket;
      mServerSocket = NULL;
    }

    mServerSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);

    int errno = mServerSocket->Listen(1);

    if (errno == 0) {
      LOG("Listen to LM SCO Server Socket is OK");
      break;
    } else {
      mServerSocket->Disconnect();
      LOG("Listen to LM SCO Server Socket failed: %d", errno);
    }
  }

  pthread_create(&(mAcceptThread), NULL, 
                 BluetoothScoManager::AcceptInternal, mServerSocket);

  return true;
}


bool
BluetoothScoManager::IsConnected()
{
  return BluetoothScoManager::sConnected;
}
