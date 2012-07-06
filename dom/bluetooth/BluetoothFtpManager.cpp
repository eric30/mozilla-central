#include "BluetoothFtpManager.h"

#include "BluetoothObexServer.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

BluetoothFtpManager::BluetoothFtpManager()
{
}

BluetoothFtpManager::~BluetoothFtpManager()
{

}

void
BluetoothFtpManager::Start()
{
  mServer = new ObexServer(BluetoothFtpManager::DEFAULT_FTP_CHANNEL, this);
}

char
BluetoothFtpManager::onConnect()
{
  LOG("FTPManager::OnConnect()");

  return ObexResponseCode::Success;
}

char
BluetoothFtpManager::onSetPath(const ObexHeaderSet& reqHeaderSet, char* response)
{
  LOG("FtpManager::OnSetPath()"); 

  // Return Success if the path exists.

  return ObexResponseCode::Success;
}

char
BluetoothFtpManager::onGet(const ObexHeaderSet& reqHeaderSet, char* response)
{
  LOG("FtpManager::OnGet()");

  int currentIndex = 3;

  for (int i = 0; i < reqHeaderSet.mCount; ++i){
    if (reqHeaderSet.mHeaders[i]->mId == ObexHeaderId::Type) {
      char* type = reqHeaderSet.mHeaders[i]->mData;
 
      // TODO(Eric)
      // xxx Temp implementation
      if (type[7] == 0x66) {
        // Folder Browsing
        LOG("GET: Folder Browsing");

        char bodyStrAscii[] =
          "<?xml version=\"1.0\"?>\r\n\
<!DOCTYPE folder-listing SYSTEM \"obex-folder-listing.dtd\">\r\n\
<folder-listing version=\"1.0\">\r\n\
<folder name=\"sdcard\" size=\"32768\" user-perm=\"RW\" modified=\"19800101T000000Z\"/>\r\n\
<file name=\"test.txt\" size=\"151\" user-perm=\"RW\" modified=\"19800101T000000Z\"/>\r\n\
</folder-listing>\r\n";

        currentIndex += AppendHeaderConnectionId(&response[currentIndex], 1);
        currentIndex += AppendHeaderBody(&response[currentIndex], bodyStrAscii, sizeof(bodyStrAscii) - 1);

        LOG("Current Index:%d", currentIndex);

        SetObexPacketInfo(response, 0xA0, currentIndex);
      } else if (type[7] == 0x63) {
        // Capability
        LOG("GET: Capability");

        currentIndex += AppendHeaderConnectionId(&response[currentIndex], 1);

        SetObexPacketInfo(response, 0xC0, currentIndex);
      }

      break;
    }
  }

  return ObexResponseCode::Success;
}

char
BluetoothFtpManager::onPut(const ObexHeaderSet& reqHeaderSet, char* response)
{
  LOG("FtpManager::OnPut()"); 

  return ObexResponseCode::Success;
}

char
BluetoothFtpManager::onDisconnect()
{
  LOG("FTPManager::OnDisconnect()"); 

  return ObexResponseCode::Success;
}
