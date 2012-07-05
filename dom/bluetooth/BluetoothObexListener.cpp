#include "BluetoothObexListener.h";

#include "ObexBase.h";

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

char
ObexListener::onConnect()
{
  return ObexResponseCode::Success;
}

char
ObexListener::onDisconnect()
{
  return ObexResponseCode::Success;
}

char 
ObexListener::onPut()
{
  return ObexResponseCode::NotImplemented;
}

char 
ObexListener::onSetPath()
{
  return ObexResponseCode::NotImplemented;
}

char 
ObexListener::onGet(const char* headerStart, int length, char* response)
{
  return ObexResponseCode::NotImplemented;
}
