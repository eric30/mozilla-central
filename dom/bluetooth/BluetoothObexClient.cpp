#include "BluetoothObexClient.h";

#include "BluetoothDevice.h";
#include "BluetoothSocket.h";

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
  char req[] = {0x80, 
                0x00, 0x0C, 
                0x10, 
                0x00, 
                0x20, 0x00,
                0xCb,
                0x00, 0x00, 0x00, mConnectionId};

  int responseCode = this->SendRequestInternal(req[0], req, sizeof(req));

  if (responseCode == 0xA0) {
    mConnected = true;
  }

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

    req[0] = (sentFileBodyLength >= fileBodyLength) ? 0x82 : 0x02;
    req[1] = currentIndex & 0xFF00;
    req[2] = currentIndex & 0x00FF;

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
  char buf[2048];

  LOG("Write request : %x", opcode);
  int ret = mSocket->WriteInternal(req, length);
 
  // Start to read
  ret = mSocket->ReadInternal(&buf[0], 1);
  LOG("Response code : %x", buf[0]);
  char responseCode = buf[0];

  ret = mSocket->ReadInternal(&buf[1], 2);
  int packetLength = (((int)buf[1]) << 8) | buf[2];
  LOG("Response packet length : %d", packetLength);

  int leftLength = packetLength - 3;
  ret = mSocket->ReadInternal(&buf[3], leftLength);

  // Start response data parsing
  if (opcode == 0x80) {
    if (responseCode != ObexResponseCode::Success) {
      LOG("Connect failed: %x", responseCode);
      mConnected = false;
    } else {
      mRemoteObexVersion = buf[3];
      mRemoteConnectionFlags = buf[4];
      mRemoteMaxPacketLength = ((buf[5] << 8) | buf[6]);

      // TODO(Eric)
      // Start header parsing, need to check header content
      ParseHeaders(&buf[7], packetLength - 7);
    }
  } else if (opcode == 0x81) {
    if (responseCode != ObexResponseCode::Success) {
      LOG("Disconnect failed: %x", responseCode);
    }
  } else if (opcode == 0x02 || opcode == 0x82) {
    // Put: Do nothing, just reply responsecode
  }
 
  return responseCode;
}

void
ObexClient::ParseHeaders(char* buf, int totalLength)
{
  char* ptr = buf;

  while (ptr - buf < totalLength) {
    char headerOpcode = *ptr++;
    int headerLength = 0;
    char highByte, lowByte;

    // IrOBEX 1.2 - 2.1 OBEX Headers
    switch (headerOpcode >> 6)
    {
      case 0x00: 
        // NULL terminated Unicode text, length prefixed with 2 byte unsigned integer.
      case 0x01:
        // byte sequence, length  prefixed with 2 byte unsigned integer.
        highByte = *ptr++;
        lowByte = *ptr++;

        headerLength = ((int)highByte << 8) | lowByte;

        break;

      case 0x02:
        // 1 byte quantity
        headerLength = 1;
        break;

      case 0x03:
        // 4 byte quantita
        headerLength = 4;
        break;
    }

    LOG("Header opcode: %x, Header length: %d", headerOpcode, headerLength);

    // Content
    ptr += headerLength;
  }
}

int
ObexClient::AppendHeaderName(char* retBuf, char* name, int length)
{
  int headerLength = length + 3;

  retBuf[0] = 0x01;
  retBuf[1] = headerLength & 0xFF00;
  retBuf[2] = headerLength & 0x00FF;

  memcpy(&retBuf[3], name, length);

  return headerLength;
}

int
ObexClient::AppendHeaderBody(char* retBuf, char* data, int length)
{
  int headerLength = length + 3;

  retBuf[0] = 0x48;
  retBuf[1] = headerLength & 0xFF00;
  retBuf[2] = headerLength & 0x00FF;

  memcpy(&retBuf[3], data, length);

  return headerLength;
}

int 
ObexClient::AppendHeaderLength(char* retBuf, int objectLength)
{
  retBuf[0] = 0xC3;
  retBuf[1] = objectLength & 0xFF000000;
  retBuf[2] = objectLength & 0x00FF0000;
  retBuf[3] = objectLength & 0x0000FF00;
  retBuf[4] = objectLength & 0x000000FF;

  return 5;
}

int
ObexClient::AppendHeaderConnectionId(char* retBuf, int connectionId)
{
  retBuf[0] = 0xCB;
  retBuf[1] = connectionId & 0xFF000000;
  retBuf[2] = connectionId & 0x00FF0000;;
  retBuf[3] = connectionId & 0x0000FF00;
  retBuf[4] = connectionId & 0x000000FF;

  return 5;
}

