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
  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);
  }

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
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);
  int newFd = socket->Accept();
  BluetoothScoManager::sConnected = true;

  return NULL;
}

bool
BluetoothScoManager::Listen()
{
  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket(BluetoothSocket::TYPE_SCO);
  }

  mSocket->Listen(1);
  pthread_create(&(mAcceptThread), NULL, 
                 BluetoothScoManager::AcceptInternal, mSocket);

  return true;
}


bool
BluetoothScoManager::IsConnected()
{
  return BluetoothScoManager::sConnected;
}
