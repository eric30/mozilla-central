/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include "BluetoothService.h"
#include "dbus/dbus.h"
#include "mozilla/ipc/DBusThread.h"
#include "mozilla/ipc/DBusUtils.h"

#include <string.h>

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

using namespace mozilla::ipc;

// TODO(Eric)
// Cannot receive any authentication related event
DBusHandlerResult agent_event_filter(DBusConnection *conn, DBusMessage *msg, void *data)
{
  if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
    LOG("%s: not interested (not a method call).", __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  LOG("%s: Received method %s:%s", __FUNCTION__,
      dbus_message_get_interface(msg), dbus_message_get_member(msg));

  return DBUS_HANDLER_RESULT_HANDLED;
}

static const DBusObjectPathVTable agent_vtable = {
  NULL, agent_event_filter, NULL, NULL, NULL, NULL
};

BEGIN_BLUETOOTH_NAMESPACE

int RegisterLocalAgent(DBusConnection* conn, const char* agentPath, const char* capabilities)
{
  DBusMessage *msg, *reply;
  DBusError err;

  if (!dbus_connection_register_object_path(conn, agentPath,
        &agent_vtable, NULL)) {
    LOG("%s: Can't register object path %s for agent!",
        __FUNCTION__, agentPath);
    return -1;
  }

  const char* adapter = get_adapter_path(conn);

  LOG("PASS, and AdapterPath = %s", adapter);

  msg = dbus_message_new_method_call("org.bluez", adapter,
                                     "org.bluez.Adapter", "RegisterAgent");
  if (!msg) {
    LOG("%s: Can't allocate new method call for agent!", __FUNCTION__);
    return -1;
  }

  dbus_message_append_args(msg,
                           DBUS_TYPE_OBJECT_PATH, &agentPath,
                           DBUS_TYPE_STRING, &capabilities,
                           DBUS_TYPE_INVALID);

  dbus_error_init(&err);
  reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
  dbus_message_unref(msg);

  if (!reply) {
    LOG("%s: Can't register agent!", __FUNCTION__);
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
    return -1;
  }

  dbus_message_unref(reply);
  dbus_connection_flush(conn);

  return 0;
}

bool 
RegisterAgent()
{
  // Register local agent
  const char *local_agent_path = "/B2G/bluetooth/agent";
  const char *capabilities = "DisplayYesNo";
  RegisterLocalAgent(GetCurrentConnection(), local_agent_path, capabilities);

  // Register agent for remote devices.
  const char *device_agent_path = "/B2G/bluetooth/remote_device_agent";

  if (!dbus_connection_register_object_path(GetCurrentConnection(), 
                                            device_agent_path,
                                            &agent_vtable, 
                                            NULL)) {
    LOG("%s: Can't register object path %s for remote device agent!",
        __FUNCTION__, device_agent_path);

    return false;
  }

  return true;
}

void 
UnregisterAgent()
{
  const char *device_agent_path = "/B2G/bluetooth/remote_device_agent";
  dbus_connection_unregister_object_path(GetCurrentConnection(), device_agent_path);
}

void 
StopDiscoveryInternal()
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

bool 
StartDiscoveryInternal()
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
const char* 
GetDefaultAdapterPath()
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

void 
GetDeviceProperties(const char* aObjectPath)
{
  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args_timeout(GetCurrentConnection(),
                                 -1, 
                                 aObjectPath,
                                 DBUS_DEVICE_IFACE, "GetProperties",
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

void 
GetAdapterProperties() 
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

bool 
SetAdapterProperty(char* propertyName, int type, void* value)
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

void
asyncCreatePairedDeviceCallback(DBusMessage *msg, void *data, void* n)
{
  DBusError err;
  dbus_error_init(&err);
  const char* backupAddress =  (const char *)data;

  if (dbus_set_error_from_message(&err, msg)) {
    LOG("Creating paired device failed, err: %s", err.name);
  } else {
    LOG("PairedDevice %s has been created", backupAddress);
  }

  dbus_error_free(&err);

  delete data;
}

void 
CreatePairedDeviceInternal(const char* aAddress, int aTimeout)
{
  const char *capabilities = "DisplayYesNo";
  const char *device_agent_path = "/B2G/bluetooth/remote_device_agent";

  char* backupAddress = new char[strlen(aAddress) + 1];
  strcpy(backupAddress, aAddress);

  // Then send CreatePairedDevice, it will register a temp device agent then 
  // unregister it after pairing process is over
  bool ret = dbus_func_args_async(GetCurrentConnection(),
      aTimeout,
      asyncCreatePairedDeviceCallback , // callback
      (void*)backupAddress,
      GetDefaultAdapterPath(),
      DBUS_ADAPTER_IFACE,
      "CreatePairedDevice",
      DBUS_TYPE_STRING, &aAddress,
      DBUS_TYPE_OBJECT_PATH, &device_agent_path,
      DBUS_TYPE_STRING, &capabilities,
      DBUS_TYPE_INVALID);
}

void 
RemoveDeviceInternal(const char* aDeviceObjectPath)
{
  bool ret = dbus_func_args_async(GetCurrentConnection(), 
      -1,
      NULL,
      NULL,
      GetDefaultAdapterPath(),
      DBUS_ADAPTER_IFACE,
      "RemoveDevice",
      DBUS_TYPE_OBJECT_PATH, &aDeviceObjectPath,
      DBUS_TYPE_INVALID);
}

void
asyncDiscoverServicesResult(DBusMessage *msg, void *data, void* n)
{
  DBusError err;
  dbus_error_init(&err);
  const char* contextPath =  (const char *)data;

  if (dbus_set_error_from_message(&err, msg)) {
    LOG("Creating paired device failed, err: %s", err.name);
  } else {
    LOG("[Discover Service] %s", contextPath);
  }

  dbus_error_free(&err);

  delete data;
}

void 
DiscoverServicesInternal(const char* aObjectPath, const char* aPattern)
{
  int len = strlen(aObjectPath) + 1;
  char* contextPath = (char *)calloc(len, sizeof(char));
  strlcpy(contextPath, aObjectPath, len);  // for callback

  LOG("... Object Path = %s", aObjectPath);

  bool ret = dbus_func_args_async(GetCurrentConnection(), 
      -1,
      asyncDiscoverServicesResult,
      contextPath,
      aObjectPath,
      DBUS_DEVICE_IFACE,
      "DiscoverServices",
      DBUS_TYPE_STRING, &aPattern,
      DBUS_TYPE_INVALID);
}

int 
AddRfcommServiceRecordInternal(const char* aName, 
                               unsigned long long aUuidMsb, 
                               unsigned long long aUuidLsb, 
                               short aChannel)
{
  LOG("... name = %s", aName);
  LOG("... uuid1 = %llX", aUuidMsb);
  LOG("... uuid2 = %llX", aUuidLsb);
  LOG("... channel = %d", aChannel);

  DBusMessage *reply = dbus_func_args(GetCurrentConnection(),
      GetDefaultAdapterPath(),
      DBUS_ADAPTER_IFACE, "AddRfcommServiceRecord",
      DBUS_TYPE_STRING, &aName,
      DBUS_TYPE_UINT64, &aUuidMsb,
      DBUS_TYPE_UINT64, &aUuidLsb,
      DBUS_TYPE_UINT16, &aChannel,
      DBUS_TYPE_INVALID);

  return reply ? dbus_returns_uint32(reply) : -1;
}

bool 
RemoveServiceRecordInternal(int aHandle) 
{
  LOG("... handle = %X", aHandle);

  DBusMessage *reply = dbus_func_args(GetCurrentConnection(),
                                      GetDefaultAdapterPath(),
                                      DBUS_ADAPTER_IFACE, "RemoveServiceRecord",
                                      DBUS_TYPE_UINT32, &aHandle,
                                      DBUS_TYPE_INVALID);

  return reply ? true : false;
}

int 
GetDeviceServiceChannelInternal(const char* aObjectPath, const char* aPattern, int aAttrId)
{
    LOG("... pattern = %s", aPattern);
    LOG("... attr_id = %#X", aAttrId);

    DBusMessage *reply = dbus_func_args(GetCurrentConnection(), aObjectPath,
                                        DBUS_DEVICE_IFACE, "GetServiceAttributeValue",
                                        DBUS_TYPE_STRING, &aPattern,
                                        DBUS_TYPE_UINT16, &aAttrId,
                                        DBUS_TYPE_INVALID);

    return reply ? dbus_returns_int32(reply) : -1;
}

END_BLUETOOTH_NAMESPACE
