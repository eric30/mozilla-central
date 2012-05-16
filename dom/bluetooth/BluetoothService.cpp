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

// TODO(Eric)
// Need refactory
const char* GetDefaultAdapterPath()
{
  DBusMessage *msg = NULL, *reply = NULL;
  DBusError err;
  const char *device_path = NULL;
  int attempt = 0;

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
    reply = dbus_connection_send_with_reply_and_block(GetCurrentConnection(), msg, -1, &err);

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
  return NULL; 
}

void GetAdapterProperties() 
{
  DBusMessage *msg, *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args_timeout(GetCurrentConnection(),
                                 -1,
                                 GetDefaultAdapterPath(),
                                 DBUS_ADAPTER_IFACE, "GetProperties",
                                 DBUS_TYPE_INVALID);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
    } else {
      LOG("DBus reply is NULL in function %s", __FUNCTION__);
    }
  }

  DBusMessageIter iter;
  if (dbus_message_iter_init(reply, &iter)) {
    // TODO(Eric)
    // No idea how to parse, but should easily parse and send it to upper layer
    
  }

  dbus_message_unref(reply);
}

void AppendVariant(DBusMessageIter *iter, int type, void *val)
{
  DBusMessageIter value_iter;
  char var_type[2] = {(char)type, '\0'};
  dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, var_type, &value_iter);
  dbus_message_iter_append_basic(&value_iter, type, val);
  dbus_message_iter_close_container(iter, &value_iter);
}

bool SetAdapterProperty(char* propertyName, int type, void* value)
{
  DBusMessage *reply, *msg;
  DBusMessageIter iter;
  DBusError err;

  /* Initialization */
  dbus_error_init(&err);

  /* Compose the command */
  msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, GetDefaultAdapterPath(),
                                     DBUS_ADAPTER_IFACE, "SetProperty");

  if (msg == NULL) {
    LOG("SetProperty : Error on creating new method call msg");
    return false;
  }

  dbus_message_append_args(msg, DBUS_TYPE_STRING, &propertyName, DBUS_TYPE_INVALID);
  dbus_message_iter_init_append(msg, &iter);
  AppendVariant(&iter, type, value);

  /* Send the command. */
  reply = dbus_connection_send_with_reply_and_block(GetCurrentConnection(), 
                                                    msg, -1, &err);
  dbus_message_unref(msg);

  if (!reply || dbus_error_is_set(&err)) {
    LOG("SetProperty : Send SetProperty Command error");
    return false;
  }

  return true;
}

END_BLUETOOTH_NAMESPACE
