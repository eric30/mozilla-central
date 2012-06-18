/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothObexManager.h"

#include "BluetoothDevice.h"
#include "BluetoothSocket.h"

#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
 
#include <unistd.h>  /* for usleep */

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

static BluetoothObexManager* sInstance = NULL;
static bool sStopAcceptThreadFlag = true;
static bool sStopEventLoopFlag = true;
static char sLastCommand = 0x00;

BluetoothObexManager::BluetoothObexManager() : mSocket(NULL)
                                           , mServerSocket(NULL)
                                           , mChannel(-1)
                                           , mAddress(NULL)
                                           , mEventThread(NULL)
                                           , mAcceptThread(NULL)
{
}

BluetoothObexManager::~BluetoothObexManager()
{
}

BluetoothObexManager*
BluetoothObexManager::GetManager()
{
  if (sInstance == NULL)
  {
    sInstance = new BluetoothObexManager();
  }

  return sInstance;
}

bool
BluetoothObexManager::WaitForConnect()
{
  if (mServerSocket != NULL) {
    LOG("Already connected or forget to clear mServerSocket?");
    return false;
  }

  mServerSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, -1, true, false, NULL);
  LOG("Created a new BluetoothServerSocket for listening");

  mChannel = BluetoothObexManager::DEFAULT_OPP_CHANNEL;

  int ret = mServerSocket->BindListen(mChannel);
  if (ret != 0) {
    LOG("BindListen failed. error no is %d", ret);
    return false;
  }

  pthread_create(&(mAcceptThread), NULL, BluetoothObexManager::AcceptInternal, mServerSocket);

  return true;
}

void 
BluetoothObexManager::StopWaiting()
{
  sStopAcceptThreadFlag = true;

  mServerSocket->CloseInternal();

  delete mServerSocket;
  mServerSocket = NULL;
}

BluetoothSocket*
BluetoothObexManager::Connect(const char* aAddress, int aChannel)
{
  if (aChannel <= 0) return NULL;
  if (IsConnected()) return NULL;

  BluetoothDevice* newDevice = new BluetoothDevice(aAddress);
  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, -1, true, false, newDevice);

  int ret = mSocket->Connect(aAddress, aChannel);
  if (ret) {
    LOG("Connect failed - OBEX RFCOMM Socket");

    delete newDevice;
    delete mSocket;
    mSocket = NULL;

    return NULL;
  } 

  LOG("Connect successfully - OBEX RFCOMM Socket");

  pthread_create(&mEventThread, NULL, BluetoothObexManager::MessageHandler, mSocket);

  //Connect
  char line[7] = {0x80, 0x00, 0x07, 0x10, 0x00, 0x20, 0x00};
  ret = write(mSocket->mFd, line, sizeof(line));

  sLastCommand = 0x80;

  return mSocket;
}

void 
BluetoothObexManager::Disconnect() 
{
  if (this->IsConnected()) {
    sStopEventLoopFlag = true;
    pthread_join(mEventThread, NULL);
    mEventThread = NULL;    
    
    LOG("Disconnect RFCOMM Socket in BluetoothHfpManager");

    mSocket->CloseInternal();
    mSocket = NULL;
  }
}

bool 
BluetoothObexManager::IsConnected() 
{
  return (mEventThread != NULL);
}

void*
BluetoothObexManager::AcceptInternal(void* ptr)
{
  BluetoothSocket* serverSocket = static_cast<BluetoothSocket*>(ptr);
  sStopAcceptThreadFlag = false;

  while (!sStopAcceptThreadFlag) {
    BluetoothSocket* newSocket = serverSocket->Accept();

    LOG("OBEX Accepted!!!");

    if (newSocket == NULL) {
      LOG("Accepted failed.");
      continue;
    }

    BluetoothObexManager* obex = BluetoothObexManager::GetManager();

    if (obex->mSocket != NULL || obex->IsConnected()) {
      LOG("A socket has been accepted, however there is no available resource.");
      newSocket->CloseInternal();

      delete newSocket;
      continue;
    }

    obex->mSocket = newSocket;

    pthread_create(&obex->mEventThread, NULL, BluetoothObexManager::MessageHandler, obex->mSocket);
  }

  return NULL;
}

void
PrintData(char* start, int length)
{
  for (int i = 0; i < length; ++i) {
    LOG("Data[%d]: %x", i, start[i]);
  }
}

int SendFakeFile(int fd)
{
  char line[48] = {0x82,
    0x00, 0x30,  //PACKET LENGTH
    0xcb,
    0x00, 0x00, 0x00, 0x01,
    0x01, // Header:Name
    0x00, 0x15, //Length of Name
    0x00, 0x74, 0x00, 0x65, 0x00, 0x73, 0x00, 0x74, 0x00, 0x2e, 0x00, 0x74,
    0x00, 0x78, 0x00, 0x74, 0x00, 0x00,
    0xc3,
    0x00, 0x00, 0x00, 0x0b,
    0x48, // Header:Body
    0x00, 0x0E, // Body length + Header
    0x45, 0x72, 0x69, 0x63, 0x20, 0x54, 0x65, 0x73, 0x74, 0x2e, 0x0a};  // Body

  int ret = write(fd, line, sizeof(line));
  sLastCommand = 0x82;
  return ret;
}

void*
BluetoothObexManager::MessageHandler(void* ptr)
{
  BluetoothSocket* socket = static_cast<BluetoothSocket*>(ptr);
  BluetoothObexManager* obex = BluetoothObexManager::GetManager();
  int ret;
  sStopEventLoopFlag = false;

  while (!sStopEventLoopFlag) {
    char buf[4096];
    char reqOpCode;

    ret = read(socket->mFd, &reqOpCode, 1);
    LOG("Msg opcode : %x", reqOpCode);

    char lengthHigh, lengthLow;
    ret = read(socket->mFd, &lengthHigh, 1);
    ret = read(socket->mFd, &lengthLow, 1);

    int pktLength = (((int)lengthHigh) << 8) | lengthLow;
    LOG("Msg Packet Length : %d", pktLength);

    int leftLength = pktLength - 3;
    ret = read(socket->mFd, buf, leftLength);

    char* line;

    switch (reqOpCode) {
      case 0x80:
        LOG("Remote sent [Connect]");

        // Response
        line = new char[7];         
        line[0] = 0xA0; // Success
        line[1] = 0x00;
        line[2] = 0x07;
        line[3] = 0x10; // Success
        line[4] = 0x00;
        line[5] = 0x04;
        line[6] = 0x00;

        ret = write(socket->mFd, line, 7);
        break;

      case 0x81:       
        LOG("Remote sent [Disconnect]");
         // Response
        line = new char[3];
        line[0] = 0xA0; // Success
        line[1] = 0x00;
        line[2] = 0x03;

        ret = write(socket->mFd, line, 3);
        break;

      case 0x02:
        LOG("Remote sent [Put] (Not final pkt)");
        // Response
        line = new char[3]; 
        line[0] = 0x90; // Success
        line[1] = 0x00;
        line[2] = 0x03;

        ret = write(socket->mFd, line, 3);
        break;

      case 0x82:
        LOG("Remote sent [Put] (Final pkt)");               
        line = new char[3];
        line[0] = 0xA0; // Success
        line[1] = 0x00;
        line[2] = 0x03;

        ret = write(socket->mFd, line, 3);
        break;

      case 0xA0:
        LOG("Remote sent [Response 'OK']");
        if (sLastCommand == 0x80) {
          SendFakeFile(socket->mFd);
        }
        line = new char[2];
        break;

      default:
        line = new char[1];
        LOG("Unhandled msg");
        break;
    }

    delete [] line;

    PrintData(buf, leftLength);
  }

  return NULL;
}
