/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#ifndef mozilla_dom_bluetooth_bluetoothsocket_h__
#define mozilla_dom_bluetooth_bluetoothsocket_h__

#include "BluetoothCommon.h"
#include "nsIDOMBluetoothSocket.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;

class BluetoothSocket : public nsIDOMBluetoothSocket
{
public:    
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLUETOOTHSOCKET

  BluetoothSocket(int aType, int aFd, bool aAuth, bool aEncrypt, BluetoothDevice* aDevice);
  ~BluetoothSocket();

  static const int TYPE_RFCOMM = 1;
  static const int TYPE_SCO = 2;
  static const int TYPE_L2CAP = 3;
  static const int RFCOMM_SO_SNDBUF = 70 * 1024;  // 70 KB send buffer

  int IsAvailable();
  int Connect(const char* aAddress, int aPort);
  void CloseInternal();
  //int WriteInternal(char* data, int length);
  int BindListen(int aPort);
  const char* GetRemoteDeviceAddress();
  int WriteInternal(const char* buf, int length);
  int ReadInternal(char* buf, int count);

  // TODO(Eric)
  // Should be exposed for developing server programs
  BluetoothSocket* Accept();

  int mFd; /* File descriptor */ 

private:
  void Init(int aFd, bool aAuth, bool aEncrypt);

  int mType;  /* one of TYPE_RFCOMM etc */
  BluetoothDevice* mDevice;    /* remote device */
  const char* mAddress;    /* remote address */
  bool mAuth;
  bool mEncrypt;
  int mPort;  /* RFCOMM channel or L2CAP psm */

  /** prevents all native calls after destroyNative() */
  bool mClosed;

  /** protects mClosed */
  //ReentrantReadWriteLock mLock;

  /** used by native code only */
  int mSocketData;
};

END_BLUETOOTH_NAMESPACE

#endif
