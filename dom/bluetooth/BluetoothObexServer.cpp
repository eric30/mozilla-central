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
    char buf[4096];
    char reqOpcode;

    ret = server->mSocket->ReadInternal(&reqOpcode, 1);
    LOG("Msg opcode : %x", reqOpcode);

    char lengthHigh, lengthLow;
    ret = server->mSocket->ReadInternal(&lengthHigh, 1);
    ret = server->mSocket->ReadInternal(&lengthLow, 1);

    int pktLength = (((int)lengthHigh) << 8) | lengthLow;
    LOG("Msg Packet Length : %d", pktLength);

    int leftLength = pktLength - 3;
    ret = server->mSocket->ReadInternal(buf, leftLength);

    char* responseBuffer = new char[255];
    int currentIndex = 3;

    if (reqOpcode == ObexRequestCode::Connect) {
      responseBuffer[0] = ObexResponseCode::Success;
      responseBuffer[3] = 0x10;  // Obex version = 1.0 = 0x10
      responseBuffer[4] = 0x00;  // flag
      responseBuffer[5] = ObexServer::MAX_PACKET_LENGTH & 0xFF00;  // maxPacketLength
      responseBuffer[6] = ObexServer::MAX_PACKET_LENGTH & 0x00FF;  // maxPacketLength

      currentIndex = 7;
      currentIndex += AppendHeaderConnectionId(&responseBuffer[currentIndex], 1);

      SetObexPacketInfo(responseBuffer, ObexResponseCode::Success, currentIndex);

      server->mSocket->WriteInternal(responseBuffer, currentIndex);
      server->mListener->onConnect();
    } else if (reqOpcode == ObexRequestCode::Disconnect) {
      SetObexPacketInfo(responseBuffer, ObexResponseCode::Success, currentIndex); 
      server->mSocket->WriteInternal(responseBuffer, currentIndex);
      server->mListener->onDisconnect();
    } else if (reqOpcode == ObexRequestCode::SetPath) {
      server->mListener->onSetPath();
    } else if (reqOpcode == ObexRequestCode::Put) {
      SetObexPacketInfo(responseBuffer, ObexResponseCode::Continue, currentIndex);
      server->mSocket->WriteInternal(responseBuffer, currentIndex);
    } else if (reqOpcode == ObexRequestCode::PutFinal) {
      SetObexPacketInfo(responseBuffer, ObexResponseCode::Success, currentIndex);
      server->mSocket->WriteInternal(responseBuffer, currentIndex);
      server->mListener->onPut();
    } else if (reqOpcode == ObexRequestCode::Get) {
      server->mListener->onGet();
    } else {
      LOG("Unhandled message: %x", reqOpcode);
    }
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
