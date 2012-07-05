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

void
BluetoothOppManager::onConnect()
{
  LOG("OPPManager::OnConnect()"); 
}

void
BluetoothOppManager::onSetPath()
{
  LOG("OPPManager::OnSetPath()"); 
}

void
BluetoothOppManager::onPut()
{
  LOG("OPPManager::OnPut()"); 
}

void
BluetoothOppManager::onGet()
{
  LOG("OPPManager::OnGet()"); 
}

void
BluetoothOppManager::onDisconnect()
{
  LOG("OPPManager::OnDisconnect()"); 
}
