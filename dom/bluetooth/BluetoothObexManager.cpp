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

  mChannel = BluetoothObexManager::DEFAULT_FTP_CHANNEL;

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

int
ResponseOfFolderListing(char** str)
{
  char test[] = "<folder-listing> </folder-listing>";
  *str = new char[sizeof(test)];

  memcpy(str, test, sizeof(test));

  return sizeof(test);
}

int
AppendHeaderWho(char* startPtr)
{
  startPtr[0] = 0x4a; // Who
  startPtr[1] = 0x00; // 2-byte length
  startPtr[2] = 0x13;
  startPtr[3] = 0xF9; // Who ID
  startPtr[4] = 0xEC;
  startPtr[5] = 0x7B;
  startPtr[6] = 0xC4;
  startPtr[7] = 0x95;
  startPtr[8] = 0x3C;
  startPtr[9] = 0x11;
  startPtr[10] = 0xD2;
  startPtr[11] = 0x98;
  startPtr[12] = 0x4E;
  startPtr[13] = 0x52;
  startPtr[14] = 0x54;
  startPtr[15] = 0x00;
  startPtr[16] = 0xDC;
  startPtr[17] = 0x9E;
  startPtr[18] = 0x09;

  return 19;
}

int
AppendHeaderConnectionId(char* startPtr, char id)
{
  startPtr[0] = 0xCB;
  startPtr[1] = 0x00;
  startPtr[2] = 0x00;
  startPtr[3] = 0x00;
  startPtr[4] = id;

  return 5;
}

int
AppendHeaderEndOfBody(char* startPtr)
{
  char bodyStrAscii[] = 
"<?xml version=\"1.0\"?>\r\n\
<!DOCTYPE folder-listing SYSTEM \"obex-folder-listing.dtd\">\r\n\
<folder-listing version=\"1.0\">\r\n\
<folder name=\"sdcard\" size=\"32768\" user-perm=\"RW\" modified=\"19800101T000000Z\"/>\r\n\
</folder-listing>\r\n";

  int bodyLength = sizeof(bodyStrAscii) + 3 - 1;

  startPtr[0] = 0x48;
  startPtr[1] = bodyLength & 0xFF00; // Length
  startPtr[2] = bodyLength & 0x00FF;

  memcpy(&startPtr[3], bodyStrAscii, bodyLength);

  return bodyLength;
}

int
GetHandler(bool finalBit, int fd, char* buf, int length)
{
  char* line = new char[2048];
  int totalPktLength = 3;

  if (buf[15] == 0x66) {
    // Folder Browsing
    LOG("GET: Folder Browsing");

    line[0] = 0xA0;

    totalPktLength += AppendHeaderConnectionId(&line[totalPktLength], 1);
    totalPktLength += AppendHeaderEndOfBody(&line[totalPktLength]);
  } else if (buf[15] == 0x63) {
    // Capability
    LOG("GET: Capability");

    line[0] = finalBit ? 0xC0 : 0x40; // Success

    totalPktLength += AppendHeaderConnectionId(&line[totalPktLength], 1);
  }

  // Last but not least, update packet length
  line[1] = totalPktLength & 0xFF00;
  line[2] = totalPktLength & 0x00FF;

  LOG("Total Length = %d", totalPktLength);

  int ret = write(fd, line, totalPktLength);
  delete [] line;

  return ret;
}

int
PutHandler(bool finalBit, int fd)
{ 
  char line[3];
  line[0] = finalBit ? 0xA0 : 0x90; // Success
  line[1] = 0x00;
  line[2] = 0x03;

  return write(fd, line, 3);
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
    char reqOpcode;

    ret = read(socket->mFd, &reqOpcode, 1);
    LOG("Msg opcode : %x", reqOpcode);

    char lengthHigh, lengthLow;
    ret = read(socket->mFd, &lengthHigh, 1);
    ret = read(socket->mFd, &lengthLow, 1);

    int pktLength = (((int)lengthHigh) << 8) | lengthLow;
    LOG("Msg Packet Length : %d", pktLength);

    int leftLength = pktLength - 3;
    ret = read(socket->mFd, buf, leftLength);

    char line[100];
    int lengthWho, lengthConnectionId;

    // Obex version = 1.0 = 0x10
    char obexVersion = 0x10;
    char flag = 0x00;
    int maxPacketLength = 2048;

    switch (reqOpcode) {
      case 0x80:
        LOG("Remote sent [Connect]");

        line[0] = 0xA0; // Success
        line[3] = obexVersion;
        line[4] = flag;
        line[5] = maxPacketLength & 0xFF00;
        line[6] = maxPacketLength & 0x00FF;

        lengthConnectionId = AppendHeaderConnectionId(&line[7], 1);
        lengthWho = AppendHeaderWho(&line[12]);

        // Total packet length
        line[1] = 0x00;
        line[2] = lengthWho + lengthConnectionId + 7;

        ret = write(socket->mFd, line, line[2]);
        break;

      case 0x81:
        LOG("Remote sent [Disconnect]");

        line[0] = 0xA0; // Success
        line[1] = 0x00;
        line[2] = 0x03;

        ret = write(socket->mFd, line, 3);
        break;

      case 0x02:
        LOG("Remote sent [Put] (Not final pkt)");
        ret = PutHandler(false, socket->mFd);
        break;

      case 0x82:
        LOG("Remote sent [Put] (Final pkt)");
        ret = PutHandler(true, socket->mFd);
        break;      
        
      case 0x03:
        LOG("Remote sent [Get] (Not final pkt)");
        ret = GetHandler(false, socket->mFd, buf, leftLength);
        break;

      case 0x83:
        LOG("Remote sent [Get] (Final pkt)");
        ret = GetHandler(true, socket->mFd, buf, leftLength);
        break;

      case 0xA0:
        LOG("Remote sent [Response 'OK']");
        if (sLastCommand == 0x80) {
          SendFakeFile(socket->mFd);
        }
        break;

      default:
        LOG("Unhandled msg");
        break;
    }

    PrintData(buf, leftLength);
  }

  return NULL;
}
