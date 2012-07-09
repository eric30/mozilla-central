#include "BluetoothServiceUuidHelper.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

USING_BLUETOOTH_NAMESPACE

int 
BluetoothServiceUuidHelper::Get16BitServiceId(unsigned long long id)
{
  int returnValue = ((id >> 32) & 0xFFFF);

  LOG("Get ID : %x", returnValue);

  return returnValue;
}
