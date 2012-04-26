#include "BluetoothScoSocket.h"

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

static const char CRLF[] = "\xd\xa";
static const int CRLF_LEN = 2;

static const int TYPE_RFCOMM = 1;
static const int TYPE_SCO = 2;
static const int TYPE_L2CAP = 3;  // TODO: Test l2cap code paths

USING_BLUETOOTH_NAMESPACE

void 
BluetoothScoSocket::InitSocketNative(int type, bool auth, bool encrypt)
{
  mType = type;
  mAuth = auth;
  mEncrypt = encrypt;

  mFd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);

  if (mFd < 0) {
    LOG("socket() failed, throwing");
    return;
  }

  LOG("...fd %d created (SCO)", mFd);

  return;
}

BluetoothScoSocket::BluetoothScoSocket() : mPort(-1), mFlag(false)
{
  InitSocketNative(TYPE_SCO, true, false);

  if (mFd <= 0) {
    LOG("Creating socket failed");
  } else {
    LOG("Creating socket succeeded");
  }
}

static
int get_bdaddr(const char *str, bdaddr_t *ba) {
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
BluetoothScoSocket::Connect(const char* bd_address)
{
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bd_address_obj;

  if (get_bdaddr(bd_address, &bd_address_obj)) {
    LOG("Cannot translate from BD_Addr string to an bdaddr_t obj");
    return false;
  }

  if (mFd <= 0) {
    LOG("Fd is not valid");
    return false;
  }

  struct sockaddr_sco addr_sco;
  addr = (struct sockaddr *)&addr_sco;
  addr_sz = sizeof(addr_sco);

  memset(addr, 0, addr_sz);
  addr_sco.sco_family = AF_BLUETOOTH;
  memcpy(&addr_sco.sco_bdaddr, &bd_address_obj, sizeof(bdaddr_t));

  int ret = connect(mFd, addr, addr_sz);
  LOG("RET = %d\n", ret);

  if (ret) {
    LOG("Connect error=%d", errno);
    return false;
  }

  return true;
}

void
BluetoothScoSocket::Listen(int channel)
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
BluetoothScoSocket::Accept()
{
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t* bdaddr;

  if (mFd <= 0) {
    LOG("Fd is not valid");
    return -1;
  } else {
    switch (mType) {
      case TYPE_RFCOMM:
        struct sockaddr_rc addr_rc;
        addr = (struct sockaddr *)&addr_rc;
        addr_sz = sizeof(addr_rc);
        bdaddr = &addr_rc.rc_bdaddr;
        memset(addr, 0, addr_sz);
        break;

      default:
        LOG("Are u kidding me");
        break;
    }

    LOG("Prepare to accept %d", mFd);

    int ret;

    do {
      // ret: a descriptor for the accepted socket
      ret = accept(mFd, addr, &addr_sz);
      LOG("Accept RET=%d, ERROR=%d", ret, errno);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
      LOG("Connect error=%d", errno);
    } else {
      // Match android_bluetooth_HeadsetBase.cpp line 384
      // Skip many lines
      // Start a thread to run an event loop
      mFlag = true;
      mFd = ret;
    }

    return ret;
  }
}

void
BluetoothScoSocket::Disconnect()
{
  close(mFd);
  LOG("Disconnected");
}

bool
BluetoothScoSocket::Available()
{
  int available;

  if (mFd <= 0) return false;

  if (ioctl(mFd, FIONREAD, &available) < 0) {
    return false;
  }

  return true;
}
