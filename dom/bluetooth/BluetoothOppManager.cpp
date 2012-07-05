#include "BluetoothOppManager.h"

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

char
BluetoothOppManager::onConnect()
{
  LOG("OPPManager::OnConnect()");

  return ObexResponseCode::Success;
}

char
BluetoothOppManager::onPut()
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
