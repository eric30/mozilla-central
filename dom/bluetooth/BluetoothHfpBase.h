/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpbase_h__
#define mozilla_dom_bluetooth_bluetoothhfpbase_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

const char* get_line(int fd, char *buf, int len,       
                     int timeout_ms, int *err);
void reply_ok(int fd);
void reply_error(int fd);
void reply_brsf(int fd, int brsf_value);
void reply_cind_current_status(int fd);
void reply_cind_range(int fd);
void reply_cmer(int fd, bool enabledindicator);
void reply_chld_range(int fd);

END_BLUETOOTH_NAMESPACE

#endif
