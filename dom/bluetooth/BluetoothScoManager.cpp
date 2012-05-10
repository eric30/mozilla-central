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

static BluetoothScoManager* sInstance = NULL;
bool BluetoothScoManager::sConnected = false;

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
BluetoothScoManager::Disconnect()
{
  if (mSocket != NULL)
  {
    mSocket->Disconnect();

    delete mSocket;
    mSocket = NULL;
  }

  BluetoothScoManager::sConnected = false;
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

//  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);
//  }

  if (mSocket->Connect(1, address)) {
    mozilla::dom::gonk::AudioManager::SetAudioRoute(3);
    BluetoothScoManager::sConnected = true;

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

  // TODO(Eric)
  // Need to let it break the while loop
  while (true) {
    int newFd = serverSocket->Accept();

    if (newFd <= 0) {
      LOG("Accept SCO connection failed, socket fd = %d", newFd);
      break;
    }

    BluetoothScoManager::sConnected = true;
  }

  return NULL;
}

bool
BluetoothScoManager::Listen()
{
  if (mServerSocket != NULL) {
    mServerSocket->Disconnect();

    delete mServerSocket;
    mServerSocket = NULL;
  }

  mServerSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);

  mServerSocket->Listen(1);
  pthread_create(&(mAcceptThread), NULL, 
                 BluetoothScoManager::AcceptInternal, mServerSocket);

  return true;
}


bool
BluetoothScoManager::IsConnected()
{
  return BluetoothScoManager::sConnected;
}
