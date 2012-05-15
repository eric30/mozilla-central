#include "BluetoothService.h"
#include "dbus/dbus.h"
#include "mozilla/ipc/DBusThread.h"
#include "mozilla/ipc/DBusUtils.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

using namespace mozilla::ipc;

static char* sAdapterPath = "";
static DBusConnection* sConnection = NULL;

BEGIN_BLUETOOTH_NAMESPACE

void StopDiscoveryInternal()
{
  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args(GetCurrentConnection(),
                         GetDefaultAdapterPath(),
                         DBUS_ADAPTER_IFACE,
                         "StopDiscovery",
                         DBUS_TYPE_INVALID);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      dbus_error_free(&err);
    }

    LOG("DBus reply is NULL in function %s\n", __FUNCTION__);
  }
}

bool StartDiscoveryInternal()
{
  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args(GetCurrentConnection(),
                         GetDefaultAdapterPath(),
                         DBUS_ADAPTER_IFACE,
                         "StartDiscovery",
                         DBUS_TYPE_INVALID);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      dbus_error_free(&err);
    }

    LOG("DBus reply is NULL in function %s\n", __FUNCTION__);

    return false;
  }

  return true;
}

const char* GetDefaultAdapterPath()
{
  DBusMessage *msg = NULL, *reply = NULL;
  DBusError err;
  const char *device_path = NULL;
  int attempt = 0;

  sConnection = GetCurrentConnection();

  for (attempt = 0; attempt < 1000 && reply == NULL; attempt ++) {
    msg = dbus_message_new_method_call("org.bluez", "/",
                                       "org.bluez.Manager", "DefaultAdapter");
    if (!msg) {
      LOG("%s: Can't allocate new method call for get_adapter_path!",
          __FUNCTION__);
      return NULL;
    }
    dbus_message_append_args(msg, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(sConnection, msg, -1, &err);

    if (!reply) {
      if (dbus_error_is_set(&err)) {
        if (dbus_error_has_name(&err,
              "org.freedesktop.DBus.Error.ServiceUnknown")) {
          // bluetoothd is still down, retry
          LOG("Service unknown\n");
          dbus_error_free(&err);
          //usleep(10000);  // 10 ms
          continue;

        } else {
          // Some other error we weren't expecting
          LOG("other error\n");
          dbus_error_free(&err);
        }
      }
    }
  }

  if (attempt == 1000) {
    LOG("timeout\n");
    goto failed;
  }

  if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH,
        &device_path, DBUS_TYPE_INVALID)
      || !device_path) {
    if (dbus_error_is_set(&err)) {
      dbus_error_free(&err);
    }
    goto failed;
  }
  dbus_message_unref(msg);
  LOG("Adapter path: %s\n", device_path);

  return device_path;

failed:
  dbus_message_unref(msg);
  return sAdapterPath; 
}

END_BLUETOOTH_NAMESPACE
