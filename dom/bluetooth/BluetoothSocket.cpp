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

#include "BluetoothSocket.h"
#include "BluetoothDevice.h"
#include "BluetoothUtils.h"

#include "nsDOMClassInfo.h"

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

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sco.h>

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

#define TYPE_AS_STR(t) \
    ((t) == TYPE_RFCOMM ? "RFCOMM" : ((t) == TYPE_SCO ? "SCO" : "L2CAP"))

USING_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothSocket, BluetoothSocket)

NS_INTERFACE_MAP_BEGIN(BluetoothSocket)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothSocket)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMBluetoothSocket)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothSocket)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(BluetoothSocket)
NS_IMPL_RELEASE(BluetoothSocket)

BluetoothSocket::BluetoothSocket(int aType, int aFd, bool aAuth, bool aEncrypt,
                                 BluetoothDevice* aDevice) : mFd(aFd)
                                                           , mType(aType)
                                                           , mDevice(aDevice)
                                                           , mAuth(aAuth)
                                                           , mEncrypt(aEncrypt)
{
  this->Init(mFd, mAuth, mEncrypt);
}

BluetoothSocket::~BluetoothSocket()
{
  if (mDevice != NULL) {
    delete mDevice;
    mDevice = NULL;
  }
}

NS_IMETHODIMP
BluetoothSocket::Close()
{
  this->CloseInternal();

  return NS_OK;
}

// ***************************************
// ********* Internal functions **********
// ***************************************

void
BluetoothSocket::Init(int aFd, bool aAuth, bool aEncrypt)
{
  int fd;
  int lm = 0;
  int sndbuf;

  if (aFd > 0) {
    LOG("Socket has been already initialized.");

    mFd = aFd;
    return;      
  }

  switch (this->mType) {
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
      LOG("Wrong TYPE = %d:%s", this->mType, __FUNCTION__);
      return;
  }

  if (fd < 0) {
    LOG("create socket() failed, should throw an exception");
    return;
  }

  /* kernel does not yet support LM for SCO */
  switch (this->mType) {
    case TYPE_RFCOMM:
      lm |= aAuth ? RFCOMM_LM_AUTH : 0;
      lm |= aEncrypt ? RFCOMM_LM_ENCRYPT : 0;
      lm |= (aAuth && aEncrypt) ? RFCOMM_LM_SECURE : 0;
      break;
    case TYPE_L2CAP:
      lm |= aAuth ? L2CAP_LM_AUTH : 0;
      lm |= aEncrypt ? L2CAP_LM_ENCRYPT : 0;
      lm |= (aAuth && aEncrypt) ? L2CAP_LM_SECURE : 0;
      break;
  }

  if (lm) {
    if (setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &lm, sizeof(lm))) {
      LOG("setsockopt(RFCOMM_LM) failed, should throw an exception");
      return;
    }
  }

  if (this->mType == TYPE_RFCOMM) {
    sndbuf = RFCOMM_SO_SNDBUF;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf))) {
      LOG("setsockopt(SO_SNDBUF) failed, should throw an exception");
      return;
    }
  }

  LOG("...fd %d created (%s, lm = %x)", fd, TYPE_AS_STR(this->mType), lm);

  mFd = fd;

  return;
}

int
BluetoothSocket::Connect(const char* aAddress, int aPort)
{
  int ret;
  bdaddr_t bdaddress;
  socklen_t addr_sz;
  struct sockaddr *addr;
  
  /* parse address into bdaddress */
  if (get_bdaddr(aAddress, &bdaddress)) {
    return -1;
  }

  switch (this->mType) {
    case TYPE_RFCOMM:
      struct sockaddr_rc addr_rc;
      addr = (struct sockaddr *)&addr_rc;
      addr_sz = sizeof(addr_rc);

      memset(addr, 0, addr_sz);
      addr_rc.rc_family = AF_BLUETOOTH;
      addr_rc.rc_channel = aPort;
      memcpy(&addr_rc.rc_bdaddr, &bdaddress, sizeof(bdaddr_t));

      break;
    case TYPE_SCO:
      struct sockaddr_sco addr_sco;
      addr = (struct sockaddr *)&addr_sco;
      addr_sz = sizeof(addr_sco);

      memset(addr, 0, addr_sz);
      addr_sco.sco_family = AF_BLUETOOTH;
      memcpy(&addr_sco.sco_bdaddr, &bdaddress, sizeof(bdaddr_t));

      break;
    case TYPE_L2CAP:
      struct sockaddr_l2 addr_l2;
      addr = (struct sockaddr *)&addr_l2;
      addr_sz = sizeof(addr_l2);

      memset(addr, 0, addr_sz);
      addr_l2.l2_family = AF_BLUETOOTH;
      addr_l2.l2_psm = aPort;
      memcpy(&addr_l2.l2_bdaddr, &bdaddress, sizeof(bdaddr_t));

      break;
    default:
      LOG("Wrong TYPE = %d:%s", this->mType, __FUNCTION__);
      return -1;
  }

  // We add parts of asocket_connect() here instead of calling it.
  do {
    ret = connect(this->mFd, addr, addr_sz);
  } while (ret && errno == EINTR);

  if (ret && errno == EINPROGRESS) {
    // TODO(Eric)
    // Need further implementation, see abort_socket.c
    LOG("Has not been implemented yet.");
  }

  LOG("...connect(%d, %s) = %d (errno %d)", this->mFd, TYPE_AS_STR(this->mType), ret, errno);

  if (ret) {
    LOG("connect error, should throw an exception");
  }

  return ret;
}

int
BluetoothSocket::BindListen(int aPort)
{
  /* Returns errno instead of throwing, so java can check errno */
  socklen_t addr_sz;
  struct sockaddr *addr;
  bdaddr_t bdaddr = *BDADDR_ANY;

  if (mFd <= 0) {
    LOG("Socket has not been initialized. Cannot BindListen.");
    return -1;
  }

  switch (mType) {
    case TYPE_RFCOMM:
      struct sockaddr_rc addr_rc;
      addr = (struct sockaddr *)&addr_rc;
      addr_sz = sizeof(addr_rc);

      memset(addr, 0, addr_sz);
      addr_rc.rc_family = AF_BLUETOOTH;
      addr_rc.rc_channel = aPort;
      memcpy(&addr_rc.rc_bdaddr, &bdaddr, sizeof(bdaddr_t));
      break;
    case TYPE_SCO:
      struct sockaddr_sco addr_sco;
      addr = (struct sockaddr *)&addr_sco;
      addr_sz = sizeof(addr_sco);

      memset(addr, 0, addr_sz);
      addr_sco.sco_family = AF_BLUETOOTH;
      memcpy(&addr_sco.sco_bdaddr, &bdaddr, sizeof(bdaddr_t));
      break;
    case TYPE_L2CAP:
      struct sockaddr_l2 addr_l2;
      addr = (struct sockaddr *)&addr_l2;
      addr_sz = sizeof(addr_l2);

      memset(addr, 0, addr_sz);
      addr_l2.l2_family = AF_BLUETOOTH;
      addr_l2.l2_psm = aPort;
      memcpy(&addr_l2.l2_bdaddr, &bdaddr, sizeof(bdaddr_t));
      break;
    default:
      return ENOSYS;
  }

  if (bind(mFd, addr, addr_sz)) {
    LOG("...bind(%d) gave errno %d", mFd, errno);
    return errno;
  }

  if (listen(mFd, 1)) {
    LOG("...listen(%d) gave errno %d", mFd, errno);
    return errno;
  }

  LOG("...bindListenNative(%d) success", mFd);

  return 0;
}

BluetoothSocket*
BluetoothSocket::Accept()
{
  struct sockaddr *addr;
  socklen_t addr_sz;
  bdaddr_t *bdaddr;

  if (mFd <= 0) {
    LOG("Socket has not been initialized. Cannot accept.");
    return NULL;
  }

  switch (mType) {
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
    case TYPE_L2CAP:
      struct sockaddr_l2 addr_l2;
      addr = (struct sockaddr *)&addr_l2;
      addr_sz = sizeof(addr_l2);
      bdaddr = &addr_l2.l2_bdaddr;
      memset(addr, 0, addr_sz);
      break;
    default:
      LOG("Invalid type value %d in: %s", mType, __FUNCTION__);
      return NULL;;
  }

  int newFd;
  do {
    // newFd: a descriptor for the accepted socket
    newFd = accept(mFd, addr, &addr_sz);
  } while (newFd < 0 && errno == EINTR);

  LOG("...accept(%d, %s) = %d (errno %d)", mFd, TYPE_AS_STR(mType), newFd, errno);

  if (newFd < 0) {
    LOG("Invalid file descriptor %d after accepting a new connection.", newFd);
    return NULL;
  }

  /* Connected - return new BluetoothSocket */
  char* bdaddrStr = new char[18];
  get_bdaddr_as_string(bdaddr, bdaddrStr);
  bdaddrStr[17] = '\0';

  LOG("Accept: Address = %s", bdaddrStr);

  BluetoothDevice* newDevice = new BluetoothDevice(bdaddrStr);
  delete bdaddrStr;

  return new BluetoothSocket(mType, newFd, mAuth, mEncrypt, newDevice);
}

void 
BluetoothSocket::CloseInternal()
{
  shutdown(this->mFd, SHUT_RDWR);
  close(this->mFd);
}

int 
BluetoothSocket::IsAvailable() 
{
  int available;

  if (ioctl(mFd, FIONREAD, &available) < 0) {
    LOG("Error occured:%s", __FUNCTION__);
    return -1;
  }

  return available;
}

const char*
BluetoothSocket::GetRemoteDeviceAddress()
{
  char* retAddress = new char[18];
  const char* currentDeviceAddress = mDevice->GetAddressInternal();
  strcpy(retAddress, currentDeviceAddress);
  retAddress[17] = '\0';

  LOG("Remote Device Address : %s", retAddress);

  delete currentDeviceAddress;

  return retAddress;
}

int 
BluetoothSocket::WriteInternal(const char* buf, int length)
{
  return write(this->mFd, buf, length);
}

int 
BluetoothSocket::ReadInternal(char* buf, int count)
{
  // TODO(Eric)
  // Should deal with returned value smaller than 'count' situation.
  return read(this->mFd, buf, count);
}
