/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothobexclient_h__
#define mozilla_dom_bluetooth_bluetoothobexclient_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothSocket;

enum ObexRequestCode {
  TEST = 0x01
};

enum ObexResponseCode {
  Continue = 0x90,
  
  Success = 0xA0,
  Created = 0xA1,
  Accepted = 0xA2,
  NonAuthoritativeInfo = 0xA3,
  NoContent = 0xA4,
  ResetContent = 0xA5,
  PartialContent = 0xA6
};

const char FINAL_BIT = 0x80;

class ObexClient
{
public:
  ObexClient(const char* aRemoteDeviceAddr, int aChannel);

  bool Init();
  bool Connect();
  bool Put(char* fileName, int fileNameLength, char* fileBody, int fileBodyLength);
  void Disconnect();

private:
  int SendRequestInternal(char opcode, char* req, int length);
  void ParseHeaders(char* buf, int totalLength);

  int AppendHeaderConnectionId(char* retBuf, int connectionId);
  int AppendHeaderName(char* retBuf, char* name, int length);
  int AppendHeaderBody(char* retBuf, char* data, int length);
  int AppendHeaderLength(char* retBuf, int length);

  BluetoothSocket* mSocket;
  BluetoothDevice* mRemoteDevice;
  char mConnectionId;
  char mRemoteBdAddr[18];
  int mRemoteChannel;
  char mRemoteObexVersion;
  char mRemoteConnectionFlags;
  int mRemoteMaxPacketLength;
  bool mConnected;
};

END_BLUETOOTH_NAMESPACE

#endif
