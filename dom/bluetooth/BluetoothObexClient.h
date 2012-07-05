/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothobexclient_h__
#define mozilla_dom_bluetooth_bluetoothobexclient_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothSocket;

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
