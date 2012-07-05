/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothobexserver_h__
#define mozilla_dom_bluetooth_bluetoothobexserver_h__

#include "BluetoothCommon.h"
#include "BluetoothObexListener.h"

#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothSocket;

class ObexServer
{
public:
  static const int MAX_PACKET_LENGTH = 0xFFFE;

  ObexServer(int aChannel, ObexListener* handlaer);
  ~ObexServer();
  
  // TODO(Eric)
  // Should not be public, however no idea for thread function to access it.
  BluetoothSocket* mServerSocket;
  BluetoothSocket* mSocket;
  ObexListener* mListener;

private:
  static void* AcceptInternal(void* ptr);
  static void* MessageHandler(void* ptr);

  BluetoothDevice* mRemoteDevice;

  pthread_t mAcceptThread;
  pthread_t mEventThread;

  int mLocalRfcommChannel;
  char mConnectionId;
  char mRemoteBdAddr[18];
  char mRemoteObexVersion;
  char mRemoteConnectionFlags;
  int mRemoteMaxPacketLength;
  bool mConnected;
};

END_BLUETOOTH_NAMESPACE

#endif
