#include "BluetoothEventHandler.h"
#include "dbus/dbus.h"
#include "mozilla/ipc/DBusThread.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

BEGIN_BLUETOOTH_NAMESPACE

BluetoothEventHandler::BluetoothEventHandler() : mAdapter(NULL)
{
  RegisterEventHandler(this);
}

BluetoothEventHandler::~BluetoothEventHandler()
{
}

void BluetoothEventHandler::Register(BluetoothAdapter* adapter)
{
  mAdapter = adapter;
}

void BluetoothEventHandler::HandleEvent(DBusMessage* msg)
{
  LOG("It's now handling event.");

  if (dbus_message_is_signal(msg, "org.bluez.Adapter", "DeviceFound")) {
    char* deviceAddress;
    DBusMessageIter iter;

    if (dbus_message_iter_init(msg, &iter)) {
      dbus_message_iter_get_basic(&iter, &deviceAddress);

      if (dbus_message_iter_next(&iter)) {
        // TODO(Eric) 
        // Parse properties
      }
    }

    //if (str_array != NULL) {
      mAdapter->onDeviceFoundNative();
    //} else {
    //  LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
    //}
  }
}

END_BLUETOOTH_NAMESPACE
