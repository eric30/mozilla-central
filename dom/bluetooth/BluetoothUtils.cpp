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

#include "BluetoothUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BEGIN_BLUETOOTH_NAMESPACE

int get_bdaddr(const char *str, bdaddr_t *ba) 
{
  char *d = ((char *)ba) + 5, *endp;
  int i;
  for(i = 0; i < 6; i++) {
    *d-- = strtol(str, &endp, 16);
    if (*endp != ':' && i != 5) {
      memset(ba, 0, sizeof(bdaddr_t));
      return -1;
    }
    str = endp + 1;
  }
  return 0;
}

void get_bdaddr_as_string(const bdaddr_t *ba, char *str) 
{
  const uint8_t *b = (const uint8_t *)ba;
  sprintf(str, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
      b[5], b[4], b[3], b[2], b[1], b[0]);
}

END_BLUETOOTH_NAMESPACE
