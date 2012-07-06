#include "BluetoothObexServer.h";

#include "BluetoothDevice.h";
#include "BluetoothSocket.h";
#include "ObexBase.h";

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

static bool sStopAcceptThreadFlag = true;
static bool sStopEventLoopFlag = true;

void*
ObexServer::MessageHandler(void* ptr)
{
  ObexServer* server = static_cast<ObexServer*>(ptr);
  int ret;
  sStopEventLoopFlag = false;

  while (!sStopEventLoopFlag) {
    char* reqHeader = new char[ObexServer::MAX_PACKET_LENGTH];
    char reqOpcode;

    ret = server->mSocket->ReadInternal(&reqOpcode, 1);
    LOG("Msg opcode : %x", reqOpcode);

    char lengthHigh, lengthLow;
    ret = server->mSocket->ReadInternal(&lengthHigh, 1);
    ret = server->mSocket->ReadInternal(&lengthLow, 1);

    int pktLength = (((int)lengthHigh) << 8) | lengthLow;
    LOG("Msg Packet Length : %d", pktLength);

    int totalHeaderLength = pktLength - 3;
    int readLength = 0;

    while (readLength < totalHeaderLength) {
      readLength += server->mSocket->ReadInternal(&reqHeader[readLength], totalHeaderLength - readLength);
      LOG("Read Length: %d, Actual Read Length: %d", totalHeaderLength, readLength);
    }

    // Start parsing header
    ObexHeaderSet reqHeaderSet(reqOpcode);
    ParseHeaders(reqHeader, totalHeaderLength, &reqHeaderSet);

    // TODO(Eric)
    // The size of responseBuffer should depend on the parameter sent from remote 
    // device in CONNECT request. Furthermore, what if upper profile would like to
    // send a file which is larger than 2048? Also needs to deal with this issue.
    char* responseBuffer = new char[2048];
    int currentIndex = 3;

    if (reqOpcode == ObexRequestCode::Connect) {
      responseBuffer[0] = server->mListener->onConnect();
      responseBuffer[3] = 0x10;  // Obex version = 1.0 = 0x10
      responseBuffer[4] = 0x00;  // flag
      responseBuffer[5] = ObexServer::MAX_PACKET_LENGTH  >> 8;  // maxPacketLength
      responseBuffer[6] = ObexServer::MAX_PACKET_LENGTH;  // maxPacketLength

      currentIndex = 7;
      currentIndex += AppendHeaderConnectionId(&responseBuffer[currentIndex], 1);

      SetObexPacketInfo(responseBuffer, responseBuffer[0], currentIndex);
    } else if (reqOpcode == ObexRequestCode::Disconnect) {
      responseBuffer[0] = server->mListener->onDisconnect();
      SetObexPacketInfo(responseBuffer, responseBuffer[0], currentIndex); 
    } else if (reqOpcode == ObexRequestCode::SetPath) {
      responseBuffer[0] = server->mListener->onSetPath(reqHeaderSet, responseBuffer);
      SetObexPacketInfo(responseBuffer, responseBuffer[0], currentIndex);
    } else if (reqOpcode == ObexRequestCode::Put) {
      SetObexPacketInfo(responseBuffer, ObexResponseCode::Continue, currentIndex);
    } else if (reqOpcode == ObexRequestCode::PutFinal) {
      responseBuffer[0] = server->mListener->onPut(reqHeaderSet, responseBuffer);
      SetObexPacketInfo(responseBuffer, responseBuffer[0], currentIndex);
    } else if (reqOpcode == ObexRequestCode::Get || reqOpcode == ObexRequestCode::GetFinal) {  
      // TODO(Eric)
      // Need to re-write
      server->mListener->onGet(reqHeaderSet, responseBuffer);
      currentIndex = (((int)responseBuffer[1]) << 8) | responseBuffer[2];
    } else {
      LOG("Unhandled message: %x", reqOpcode);
      SetObexPacketInfo(responseBuffer, ObexResponseCode::BadRequest, currentIndex);
    }

    server->mSocket->WriteInternal(responseBuffer, currentIndex);

    delete [] reqHeader;
  }

  return NULL;
}

void*
ObexServer::AcceptInternal(void* ptr)
{
  ObexServer* server = static_cast<ObexServer*>(ptr);
  sStopAcceptThreadFlag = false;

  while (!sStopAcceptThreadFlag) {
    // TODO(Eric)
    // we should(?) only allow one connection at a time.
    server->mSocket = server->mServerSocket->Accept();

    if (server->mSocket == NULL) {
      LOG("Accepted failed.");
      continue;
    }

    LOG("OBEX connection accepted");

    pthread_create(&server->mEventThread, NULL, ObexServer::MessageHandler, ptr);
  }

  return NULL;
}

ObexServer::ObexServer(int aChannel, ObexListener* aListener) : mLocalRfcommChannel(aChannel)
                                                              , mListener(aListener)
{
  mServerSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, -1, true, false, NULL);

  int ret = mServerSocket->BindListen(mLocalRfcommChannel);
  if (ret != 0) {
    delete mServerSocket;
    mServerSocket = NULL;

    LOG("BindListen failed. error no is %d", ret);
  } else {
    pthread_create(&(mAcceptThread), NULL, ObexServer::AcceptInternal, this);
  }
}

ObexServer::~ObexServer()
{
  sStopEventLoopFlag = true;
  pthread_join(mEventThread, NULL);
  mEventThread = NULL;

  sStopAcceptThreadFlag = true;
  pthread_join(mAcceptThread, NULL);
  mAcceptThread = NULL;

  mSocket->CloseInternal();
  mSocket = NULL;
}
