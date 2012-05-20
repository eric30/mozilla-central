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

#include "BluetoothHfpBase.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/uio.h>

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

BEGIN_BLUETOOTH_NAMESPACE

static const char CRLF[] = "\xd\xa";
static const int CRLF_LEN = 2;

static inline int write_error_check(int fd, const char* line, int len) {
  int ret;
  errno = 0;
  ret = write(fd, line, len);
  if (ret < 0) {
    LOG("%s: write() failed: %s (%d)", __FUNCTION__, strerror(errno),
        errno);
    return -1;
  }
  if (ret != len) {
    LOG("%s: write() only wrote %d of %d bytes", __FUNCTION__, ret, len);
    return -1;
  }
  return 0;
}

static int is_ascii(char *line) {
  for (;;line++) {
    if (*line == 0) return 1;
    if (*line >> 7) return 0;
  }
}

const char* get_line(int fd, char *buf, int len, 
                     int timeout_ms, int *err)
{
  char *bufit=buf;
  int fd_flags = fcntl(fd, F_GETFL, 0);
  struct pollfd pfd;

again:
  *bufit = 0;
  pfd.fd = fd;
  pfd.events = POLLIN;
  *err = errno = 0;
  int ret = poll(&pfd, 1, timeout_ms);
  if (ret < 0) {
    LOG("poll() error\n");
    *err = errno;
    return NULL;
  }
  if (ret == 0) {
    return NULL;
  }

  if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
    LOG("RFCOMM poll() returned  success (%d), "
        "but with an unexpected revents bitmask: %#x\n", ret, pfd.revents);
    errno = EIO;
    *err = errno;
    return NULL;
  }

  while ((int)(bufit - buf) < (len - 1))
  {
    errno = 0;
    int rc = read(fd, bufit, 1);

    if (!rc)
      break;

    if (rc < 0) {
      if (errno == EBUSY) {
        LOG("read() error %s (%d): repeating read()...",
            strerror(errno), errno);
        goto again;
      }
      *err = errno;
      LOG("read() error %s (%d)", strerror(errno), errno);
      return NULL;
    }


    if (*bufit=='\xd') {
      break;
    }

    if (*bufit=='\xa')
      bufit = buf;
    else
      bufit++;
  }

  *bufit = NULL;
  // Simple validation. Must be all ASCII.
  // (we sometimes send non-ASCII UTF-8 in address book, but should
  // never receive non-ASCII UTF-8).
  // This was added because of the BMW 2005 E46 which sends binary junk.
  if (is_ascii(buf)) {
    LOG("Bluetooth AT recv : %s", buf);
  } else {
    LOG("Ignoring invalid AT command: %s", buf);
    buf[0] = NULL;
  }

  return buf;
}

static int send_line(int fd, const char* line) {
  int nw;
  int len = strlen(line);
  int llen = len + CRLF_LEN * 2 + 1;
  char *buffer = (char *)calloc(llen, sizeof(char));

  snprintf(buffer, llen, "%s%s%s", CRLF, line, CRLF);

  if (write_error_check(fd, buffer, llen - 1)) {
    free(buffer);
    return -1;
  }
  free(buffer);
  return 0;
}

void reply_ok(int fd)
{
  if (send_line(fd, "OK") != 0) {
    LOG("Reply [OK] failed");
  }
}

void reply_error(int fd)
{
  if (send_line(fd, "ERROR") != 0) {
    LOG("Reply [ERROR] failed");
  }
}

void reply_brsf(int fd)
{
  if (send_line(fd, "+BRSF: 23") != 0) {
    LOG("Reply +BRSF failed");
  }
}

void reply_cind_current_status(int fd)
{
  const char* str = "+CIND: 1,0,0,0,3,0,3";

  if (send_line(fd, str) != 0) {
    LOG("Reply +CIND failed");
  }
}

void reply_cind_range(int fd)
{
  const char* str = "+CIND: (\"service\",(0-1)),(\"call\",(0-1)),(\"callsetup\",(0-3)), \
                     (\"callheld\",(0-2)),(\"signal\",(0-5)),(\"roam\",(0-1)), \
                     (\"battchg\",(0-5))";

  if (send_line(fd, str) != 0) {
    LOG("Reply +CIND=? failed");
  }
}

void reply_cmer(int fd, bool enableIndicator)
{
  const char* str = enableIndicator ? "+CMER: 3,0,0,1" : "+CMER: 3,0,0,0";

  if (send_line(fd, str) != 0) {
    LOG("Reply +CMER= failed");
  }
}

void reply_chld_range(int fd)
{
  const char* str = "+CHLD: (0,1,2,3)";

  if (send_line(fd, str) != 0) {
    LOG("Reply +CHLD=? failed");
  }
}

END_BLUETOOTH_NAMESPACE
