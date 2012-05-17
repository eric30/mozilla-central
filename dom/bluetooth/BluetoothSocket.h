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

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket 
{
public:
  BluetoothSocket();
  ~BluetoothSocket();

  int IsAvailable();

private:
  int mFd; /* File description */ 
  int mType;  /* one of TYPE_RFCOMM etc */
  //BluetoothDevice mDevice;    /* remote device */
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
