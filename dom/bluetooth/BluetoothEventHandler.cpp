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
  DBusError err;
  dbus_error_init(&err);

  LOG("%s: Received signal %s:%s from %s", __FUNCTION__,
      dbus_message_get_interface(msg), dbus_message_get_member(msg),
      dbus_message_get_path(msg));

  if (dbus_message_is_signal(msg, "org.bluez.Adapter", "DeviceFound")) {
    char* deviceAddress;
    DBusMessageIter iter;
    bool handled = false;

    if (dbus_message_iter_init(msg, &iter)) {
      dbus_message_iter_get_basic(&iter, &deviceAddress);

      if (dbus_message_iter_next(&iter)) {
        // TODO(Eric) 
        // Parse properties
        handled = true;
      }
    }

    //if (handled) {
      mAdapter->onDeviceFoundNative(deviceAddress);
    //} else {
      //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
    //}
  } else if (dbus_message_is_signal(msg, "org.bluez.Adapter", "DeviceCreated")) {
    char *deviceObjectPath;
    if (dbus_message_get_args(msg, &err,
                              DBUS_TYPE_OBJECT_PATH, &deviceObjectPath,
                              DBUS_TYPE_INVALID)) {
      mAdapter->onDeviceCreatedNative(deviceObjectPath);
    } else {
      //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
    }
  }
}

END_BLUETOOTH_NAMESPACE
