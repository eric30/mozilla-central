#include "BluetoothOppManager.h"

#include "BluetoothObexClient.h"
#include "BluetoothObexServer.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

BluetoothOppManager::BluetoothOppManager()
{
}

BluetoothOppManager::~BluetoothOppManager()
{

}

void
BluetoothOppManager::Start()
{
  mServer = new ObexServer(BluetoothOppManager::DEFAULT_OPP_CHANNEL, this);
}

// TODO(Eric)
// Should channel be sent from outside or should it be queried inside SendFile()?
void
BluetoothOppManager::SendFile(const char* aRemoteDeviceAddr, int aChannel, char* filePath)
{
  ObexClient* client = new ObexClient(aRemoteDeviceAddr, aChannel);

  if (client->Init()) {
    if (client->Connect()) {
      // client->Put(filePath, sizeof(filePath), ..., ...);
    }
  } else {
    LOG("Client initialization failed.");
  }

  delete client;
}

char
BluetoothOppManager::onConnect()
{
  LOG("OPPManager::OnConnect()");

  return ObexResponseCode::Success;
}

char
BluetoothOppManager::onPut(const ObexHeaderSet& reqHeaderSet, char* response)
{
  LOG("OPPManager::OnPut()");
  

  return ObexResponseCode::Success;
}

char
BluetoothOppManager::onDisconnect()
{
  LOG("OPPManager::OnDisconnect()"); 

  return ObexResponseCode::Success;
}
