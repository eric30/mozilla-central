/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothuuid_h__
#define mozilla_dom_bluetooth_bluetoothuuid_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

namespace BluetoothServiceUuid {
  const char* AudioSink     = "0000110B-0000-1000-8000-00805F9B34FB";
  const char* AudioSource   = "0000110A-0000-1000-8000-00805F9B34FB";
  const char* AdvAudioDist  = "0000110D-0000-1000-8000-00805F9B34FB";
  const char* Headset       = "00001108-0000-1000-8000-00805F9B34FB";
  const char* Handsfree     = "0000111E-0000-1000-8000-00805F9B34FB";
}

END_BLUETOOTH_NAMESPACE

#endif
