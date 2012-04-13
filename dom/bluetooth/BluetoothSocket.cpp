#include "BluetoothSocket.h"

#include "bluetooth.h"
#include "rfcomm.h"
#include "l2cap.h"
#include "sco.h"

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

#define TYPE_AS_STR(t) \
    ((t) == TYPE_RFCOMM ? "RFCOMM" : ((t) == TYPE_SCO ? "SCO" : "L2CAP"))

static const int TYPE_RFCOMM = 1;
static const int TYPE_SCO = 2;
static const int TYPE_L2CAP = 3;  // TODO: Test l2cap code paths
static const int RFCOMM_SO_SNDBUF = 70 * 1024;  // 70 KB send buffer

void 
BluetoothSocket::InitSocketNative()
{
  int fd;
  int lm = 0;
  int sndbuf;
  bool auth = true;
  bool encrypt = true;
  int type = TYPE_RFCOMM;

  switch (type) {
    case TYPE_RFCOMM:
      fd = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
      break;
    case TYPE_SCO:
      fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
      break;
    case TYPE_L2CAP:
      fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
      break;
    default:
      return;
  }

  if (fd < 0) {
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
    if (setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
      LOG("setsockopt(RFCOMM_LM) failed, throwing");
      return;
    }
  }

  if (type == TYPE_RFCOMM) {
    sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      LOG("setsockopt(SO_SNDBUF) failed, throwing");
      return;
    }
  }

  LOG("...fd %d created (%s, lm = %x)", fd, TYPE_AS_STR(type), lm);

  mFd = (int)fd;

  return;
}

BluetoothSocket::BluetoothSocket()
{
  mPort = -1;
  mType = TYPE_RFCOMM;
  mFd = -1;
  mAuth = true;
  mEncrypt = true;
  // mUuid = ?  should be uuid of HFP
  // mAddress = a8:26:d9:df:64:7a

  InitSocketNative();

  if (mFd <= 0) {
    LOG("Creating socket failed");
  } else {
    LOG("Creating socket succeeded");
  }

/*
  mInputStream = new BluetoothInputStream(this);
  mOutputStream = new BluetoothOutputStream(this);
  mClosed = false;
  mLock = new ReentrantReadWriteLock();
*/
}

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

void
BluetoothSocket::Connect(int channel, const char* bd_address)
{
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bd_address_obj;

  mPort = channel;

  if (get_bdaddr(bd_address, &bd_address_obj)) {
    LOG("Terrible");
    return;
  }

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

    int ret = connect(mFd, addr, addr_sz);
    LOG("RET = %d\n", ret);

    if (ret) {
      LOG("Connect error");
    }

    return;
  }
}
