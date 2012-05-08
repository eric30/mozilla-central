/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothsocket_h__
#define mozilla_dom_bluetooth_bluetoothsocket_h__

#include "BluetoothCommon.h"

#include <pthread.h>

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket {
public:
  static const int TYPE_RFCOMM = 1;
  static const int TYPE_SCO = 2;
  static const int TYPE_L2CAP = 3;

  int mPort;
  int mType;
  int mFd;
  bool mAuth;
  bool mEncrypt;

  BluetoothSocket(int type);
  bool Connect(int channel, const char* bd_address);
  void Disconnect();
  void Listen(int channel);
  int Accept();
  bool Available();
  const char* GetAddress();
  static const char* get_line(int fd, char *buf, int len, int timeout_ms, int *err);
  static int send_line(int fd, const char* line);

protected:
  pthread_t mThread;
  pthread_t mAcceptThread;
  char mAddress[18];
  void InitSocketNative(int type, bool auth, bool encrypt);
};

END_BLUETOOTH_NAMESPACE

#endif
