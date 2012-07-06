#include "BluetoothObexClient.h";

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

ObexClient::ObexClient(const char* aRemoteDeviceAddr, int aChannel) : mRemoteChannel(aChannel)
                                                                    , mConnected(false)
                                                                    , mSocket(NULL)
                                                                    , mRemoteDevice(NULL)
                                                                    , mConnectionId(0)
                                                                    , mRemoteMaxPacketLength(255)
{
  strcpy(mRemoteBdAddr, aRemoteDeviceAddr);
}

bool ObexClient::Init()
{
  // Initialize BluetoothSocket
  mRemoteDevice = new BluetoothDevice(mRemoteBdAddr);
  mSocket = new BluetoothSocket(BluetoothSocket::TYPE_RFCOMM, -1, true, false, mRemoteDevice);

  int ret = mSocket->Connect(mRemoteBdAddr, mRemoteChannel);
  if (ret) {
    LOG("Connect failed - OBEX RFCOMM Socket");

    delete mRemoteDevice;
    delete mSocket;

    mSocket = NULL;
    mRemoteDevice = NULL;

    return false;
  }
    
  LOG("Connect successfully - OBEX RFCOMM Socket");

  return true;
}

bool 
ObexClient::Connect()
{
  if (mSocket == NULL) return false;
  if (mConnected) return false;

  ++mConnectionId;

  // Connect request
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2][Headers:var]
  char req[255];
  int currentIndex = 7;

  req[3] = 0x10; // version:1.0
  req[4] = 0x00; // flag:0x00
  req[5] = ObexClient::MAX_PACKET_LENGTH >> 8;
  req[6] = ObexClient::MAX_PACKET_LENGTH;

  currentIndex += AppendHeaderConnectionId(&req[currentIndex], mConnectionId);
  SetObexPacketInfo(req, 0xA0, currentIndex);

  if (this->SendRequestInternal(req[0], req, sizeof(req)) == 0xA0) mConnected = true;

  return true;
}

// The encoding of filename is Unicode, so we need the 2nd parameter to get length.
bool ObexClient::Put(char* fileName, int fileNameLength, char* fileBody, int fileBodyLength)
{
  if (mSocket == NULL) return false;
  if (!mConnected) return false;

  char* req = new char[mRemoteMaxPacketLength];

  int sentFileBodyLength = 0;
  int currentIndex = 3;

  currentIndex += AppendHeaderConnectionId(&req[currentIndex], mConnectionId);
  currentIndex += AppendHeaderName(&req[currentIndex], fileName, fileNameLength);
  currentIndex += AppendHeaderLength(&req[currentIndex], fileBodyLength);

  while (fileBodyLength > sentFileBodyLength) {
    int packetLeftSpace = mRemoteMaxPacketLength - currentIndex - 3;

    if (fileBodyLength <= packetLeftSpace) {
      currentIndex += AppendHeaderBody(&req[currentIndex], &fileBody[sentFileBodyLength], fileBodyLength);
      sentFileBodyLength += fileBodyLength;
    } else {
      currentIndex += AppendHeaderBody(&req[currentIndex], &fileBody[sentFileBodyLength], packetLeftSpace);
      sentFileBodyLength += packetLeftSpace;
    }

    LOG("Sent file body length: %d", sentFileBodyLength);

    if (sentFileBodyLength >= fileBodyLength) {
      SetObexPacketInfo(req, ObexRequestCode::PutFinal, currentIndex);
    } else {
      SetObexPacketInfo(req, ObexRequestCode::Put, currentIndex);
    }

    int responseCode = this->SendRequestInternal(req[0], req, currentIndex);
    if (responseCode != 0x90 && responseCode != 0xA0) {
      LOG("MUST BE something wrong, the response code should be 0x90 or 0xA0.");
      return false;
    }

    currentIndex = 3;
  }

  delete [] req;

  return true;
}

void 
ObexClient::Disconnect()
{
  if (mConnected) {
    // Disconnect request
    char req[] = {0x81, 0x00, 0x03};
    int responseCode = this->SendRequestInternal(req[0], req, sizeof(req));

    LOG("ResponseCode of Disconnect : %x", responseCode);

    mConnected = false;
  }
}

int 
ObexClient::SendRequestInternal(char opcode, char* req, int length)
{
  LOG("Write request : %x", opcode);
  int ret = mSocket->WriteInternal(req, length);
 
  // Start to read
  char* buf = new char[ObexClient::MAX_PACKET_LENGTH];

  ret = mSocket->ReadInternal(&buf[0], 1);
  LOG("Response code : %x", buf[0]);
  char responseCode = buf[0];

  ret = mSocket->ReadInternal(&buf[1], 2);
  int packetLength = (((int)buf[1]) << 8) | buf[2];
  LOG("Response packet length : %d", packetLength);

  int leftLength = packetLength - 3;
  ret = mSocket->ReadInternal(&buf[3], leftLength);

  // Start parsing response data
  ObexHeaderSet headerSet(opcode);

  if (opcode == ObexRequestCode::Connect) {
    if (responseCode != ObexResponseCode::Success) {
      LOG("Connect failed: %x", responseCode);
      mConnected = false;
    } else {
      mRemoteObexVersion = buf[3];
      mRemoteConnectionFlags = buf[4];
      mRemoteMaxPacketLength = ((buf[5] << 8) | buf[6]);

      ParseHeaders(&buf[7], packetLength - 7, &headerSet);

      // TODO(Eric) 
      // Remember to check if header "Connection Id" exists in headerSet
    }
  } else if (opcode == ObexRequestCode::Disconnect) {
    if (responseCode != ObexResponseCode::Success) {
      LOG("Disconnect failed: %x", responseCode);
    }
  } else if (opcode == ObexRequestCode::Put || opcode == ObexRequestCode::PutFinal) {
    // Put: Do nothing, just reply responsecode
  }
 
  delete [] buf;

  return responseCode;
}

