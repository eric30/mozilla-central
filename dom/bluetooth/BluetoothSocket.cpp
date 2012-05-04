#include "BluetoothSocket.h"

#include "bluetooth.h"
#include "rfcomm.h"
#include "l2cap.h"
#include "sco.h"

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
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

#define TYPE_AS_STR(t) \
    ((t) == TYPE_RFCOMM ? "RFCOMM" : ((t) == TYPE_SCO ? "SCO" : "L2CAP"))

static const char CRLF[] = "\xd\xa";
static const int CRLF_LEN = 2;

static const int RFCOMM_SO_SNDBUF = 70 * 1024;  // 70 KB send buffer

USING_BLUETOOTH_NAMESPACE

void 
BluetoothSocket::InitSocketNative(int type, bool auth, bool encrypt)
{
  int lm = 0;
  int sndbuf;

  mType = type;
  mAuth = auth;
  mEncrypt = encrypt;

  switch (type) {
    case TYPE_RFCOMM:
      mFd = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      break;
    case TYPE_SCO:
      mFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
      break;
    case TYPE_L2CAP:
      mFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
      break;
    default:
      return;
  }

  if (mFd < 0) {
    LOG("socket() failed, throwing");
    return;
  }

  /* kernel does not yet support LM for SCO */
  switch (type) {
    case TYPE_RFCOMM:
      lm |= auth ? RFCOMM_LM_AUTH : 0;
      lm |= encrypt ? RFCOMM_LM_ENCRYPT : 0;
      lm |= (auth && encrypt) ? RFCOMM_LM_SECURE : 0;
      break;
    case TYPE_L2CAP:
      lm |= auth ? L2CAP_LM_AUTH : 0;
      lm |= encrypt ? L2CAP_LM_ENCRYPT : 0;
      lm |= (auth && encrypt) ? L2CAP_LM_SECURE : 0;
      break;
  }

  if (lm) {
    if (setsockopt(mFd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
      LOG("setsockopt(RFCOMM_LM) failed, throwing");
      return;
    }
  }

  if (type == TYPE_RFCOMM) {
    sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(mFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      LOG("setsockopt(SO_SNDBUF) failed, throwing");
      return;
    }
  }

  LOG("...fd %d created (%s, lm = %x)", mFd, TYPE_AS_STR(type), lm);

  return;
}

BluetoothSocket::BluetoothSocket(int type) : mPort(-1)
{
  InitSocketNative(type, true, false);

  if (mFd <= 0) {
    LOG("Creating socket failed");
  } else {
    LOG("Creating socket succeeded");
  }
}

int 
get_bdaddr(const char *str, bdaddr_t *ba) {
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

bool
BluetoothSocket::Connect(int channel, const char* bd_address)
{
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bd_address_obj;

  mPort = channel;

  if (get_bdaddr(bd_address, &bd_address_obj)) {
    LOG("Terrible");
    return false;
  }

  if (mFd <= 0) {
    LOG("Fd is not valid");
    return false;
  }

  switch (mType) {
    case TYPE_RFCOMM:
      struct sockaddr_rc addr_rc;
      addr = (struct sockaddr *)&addr_rc;
      addr_sz = sizeof(addr_rc);

      memset(addr, 0, addr_sz);
      addr_rc.rc_family = AF_BLUETOOTH;
      addr_rc.rc_channel = mPort;
      memcpy(&addr_rc.rc_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
      break;
    case TYPE_SCO:
      struct sockaddr_sco addr_sco;
      addr = (struct sockaddr *)&addr_sco;
      addr_sz = sizeof(addr_sco);

      memset(addr, 0, addr_sz);
      addr_sco.sco_family = AF_BLUETOOTH;
      memcpy(&addr_sco.sco_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
      break;
    default:
      LOG("Are u kidding me");
      break;
  }

  int ret = connect(mFd, addr, addr_sz);
  LOG("RET = %d\n", ret);

  if (ret) {
    LOG("Connect error=%d", errno);
    return false;
  }

  // Match android_bluetooth_HeadsetBase.cpp line 384
  // Skip many lines
  return true;
}

const char* 
BluetoothSocket::get_line(int fd, char *buf, int len, int timeout_ms, int *err) {
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

  // According to ITU V.250 section 5.1, IA5 7 bit chars are used,
  //   the eighth bit or higher bits are ignored if they exists
  // We mask out only eighth bit, no higher bit, since we do char
  // string here, not wide char.
  // We added this processing due to 2 real world problems.
  // 1 BMW 2005 E46 which sends binary junk
  // 2 Audi 2010 A3, dial command use 0xAD (soft-hyphen) as number
  //   formater, which was rejected by the AT handler
  //mask_eighth_bit(buf);

  return buf;
}

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

int BluetoothSocket::send_line(int fd, const char* line) {
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

void
BluetoothSocket::Listen(int channel)
{
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bd_address_obj = *BDADDR_ANY;

  mPort = channel;

  if (mFd <= 0) {
    LOG("Fd is not valid");
  } else {
    switch (mType) {
      case TYPE_RFCOMM:
        struct sockaddr_rc addr_rc;
        addr = (struct sockaddr *)&addr_rc;
        addr_sz = sizeof(addr_rc);

        memset(addr, 0, addr_sz);
        addr_rc.rc_family = AF_BLUETOOTH;
        addr_rc.rc_channel = mPort;
        memcpy(&addr_rc.rc_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
        break;
      case TYPE_SCO:
        struct sockaddr_sco addr_sco;
        addr = (struct sockaddr *)&addr_sco;
        addr_sz = sizeof(addr_sco);

        memset(addr, 0, addr_sz);
        addr_sco.sco_family = AF_BLUETOOTH;
        memcpy(&addr_sco.sco_bdaddr, &bd_address_obj, sizeof(bdaddr_t));
        break;
      default:
        LOG("Are u kidding me");
        break;
    }

    if (bind(mFd, addr, addr_sz)) {
      LOG("...bind(%d) gave errno %d", mFd, errno);
    }

    if (listen(mFd, 1)) {
      LOG("...listen(%d) gave errno %d", mFd, errno);
    }

    LOG("...bindListenNative(%d) success", mFd);

    return;
  }
}

int
BluetoothSocket::Accept()
{
  BluetoothSocket* socket = this;

  int ret;
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t* bdaddr;

  switch (socket->mType) {
    case TYPE_RFCOMM:
      struct sockaddr_rc addr_rc;
      addr = (struct sockaddr *)&addr_rc;
      addr_sz = sizeof(addr_rc);
      bdaddr = &addr_rc.rc_bdaddr;
      memset(addr, 0, addr_sz);
      break;
    case TYPE_SCO:
      struct sockaddr_sco addr_sco;
      addr = (struct sockaddr *)&addr_sco;
      addr_sz = sizeof(addr_sco);
      bdaddr = &addr_sco.sco_bdaddr;
      memset(addr, 0, addr_sz);
      break;
    default:
      LOG("Are u kidding me?");
      break;
  }

  do {
    // ret: a descriptor for the accepted socket
    ret = accept(socket->mFd, addr, &addr_sz);
    LOG("Accept RET=%d, ERROR=%d", ret, errno);
  } while (ret < 0 && errno == EINTR);

  if (ret < 0) {
    LOG("Connect error=%d", errno);
  } else {
    // Match android_bluetooth_HeadsetBase.cpp line 384
    // Skip many lines
    // Start a thread to run an event loop
    socket->mFd = ret;
    //pthread_create(&(socket->mThread), NULL, BluetoothSocket::StartEventThread, ptr);
  }

  return ret;
}

/*
int
BluetoothSocket::Accept()
{
  if (mFd <= 0) {
    LOG("Fd is not valid");
    return -1;
  }

  pthread_create(&(mAcceptThread), NULL, BluetoothSocket::AcceptInternal, this);

  return 0;
}
*/

void
BluetoothSocket::Disconnect()
{
  void* ret;

  LOG("Disconnect: FD=%d", mFd);

  pthread_join(mThread, &ret);
  close(mFd);

  LOG("Disconnected");
}

bool
BluetoothSocket::Available()
{
  int available;

  if (mFd <= 0) return false;

  if (ioctl(mFd, FIONREAD, &available) < 0) {
    return false;
  }

  return true;
}
