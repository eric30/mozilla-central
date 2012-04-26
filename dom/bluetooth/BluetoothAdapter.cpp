/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothAdapter.h"
#include "BluetoothProperties.h"
#include "BluetoothEvent.h"
#include "BluetoothSocket.h"
#include "BluetoothScoSocket.h"
#include "BluetoothServiceUuid.h"
#include "BluetoothUtils.h"

#include "AudioManager.h"
#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"
#include <dlfcn.h>
#include <string.h>
#include "dbus/dbus.h"
#include "jsapi.h"

#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args); printf("\n");
#endif

static void
FireEnabled(bool aResult, nsIDOMDOMRequest* aDomRequest)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");

  if (!rs) {
    NS_WARNING("No DOMRequest Service!");
    return;
  }

  mozilla::DebugOnly<nsresult> rv = aResult ?     
    rs->FireSuccess(aDomRequest, JSVAL_VOID) :
    rs->FireError(aDomRequest, 
                  NS_LITERAL_STRING("Bluetooth firmware loading failed"));

  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Bluetooth firmware loading failed");
}

USING_BLUETOOTH_NAMESPACE

template<typename T>
bool GetDBusDictValue(DBusMessageIter& iter, const char* name, int expected_type, T& val) {
  DBusMessageIter dict_entry, dict;
  int size = 0,array_index = 0;
  int len = 0, prop_type = DBUS_TYPE_INVALID, prop_index = -1, type;

  if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
    LOG("This isn't a dictionary!\n");
    return false;
  }
  dbus_message_iter_recurse(&iter, &dict);
  do {
    len = 0;
    if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_DICT_ENTRY)
    {
      LOG("Not a dict entry!\n");
      return false;
    }            
    dbus_message_iter_recurse(&dict, &dict_entry);
    DBusMessageIter prop_val, array_val_iter;
    char *property = NULL;
    uint32_t array_type;
    char *str_val;
    int i, j, type, int_val;

    if (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_STRING) {
      LOG("Iter not a string!\n");
      return false;
    }
    dbus_message_iter_get_basic(&dict_entry, &property);
    LOG("Property: %s\n", property);
    if(strcmp(property, name) != 0) continue;
    if (!dbus_message_iter_next(&dict_entry))
    {
      LOG("No next!\n");
      return false;
    }
    if (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_VARIANT)
    {
      LOG("Iter not a variant!\n");
      return false;
    }
    dbus_message_iter_recurse(&dict_entry, &prop_val);
    // type = properties[*prop_index].type;
    if (dbus_message_iter_get_arg_type(&prop_val) != expected_type) {
      //   LOGE("Property type mismatch in get_property: %d, expected:%d, index:%d",
      //        dbus_message_dict_entry_get_arg_type(&prop_val), type, *prop_index);
      LOG("Not the type we expect!\n");
      return false;
    }
    dbus_message_iter_get_basic(&prop_val, &val);
    return true;
  } while(dbus_message_iter_next(&dict));
  return false;
}

static struct BluedroidFunctions {
  bool initialized;
  bool tried_initialization;

  BluedroidFunctions() :
    initialized(false),
    tried_initialization(false)
  {
  }
  
  int (* bt_enable)();
  int (* bt_disable)();
  int (* bt_is_enabled)();
} sBluedroidFunctions;

static bool EnsureBluetoothInit() {
  if (sBluedroidFunctions.tried_initialization)
  {
    return sBluedroidFunctions.initialized;
  }

  sBluedroidFunctions.initialized = false;
  sBluedroidFunctions.tried_initialization = true;
  
  void* handle = dlopen("libbluedroid.so", RTLD_LAZY);

  if(!handle) {
    NS_ERROR("Failed to open libbluedroid.so, bluetooth cannot run");
    return false;
  }

  sBluedroidFunctions.bt_enable = (int (*)())dlsym(handle, "bt_enable");
  if(sBluedroidFunctions.bt_enable == NULL) {
    NS_ERROR("Failed to attach bt_enable function");
    return false;
  }
  sBluedroidFunctions.bt_disable = (int (*)())dlsym(handle, "bt_disable");
  if(sBluedroidFunctions.bt_disable == NULL) {
    NS_ERROR("Failed to attach bt_disable function");
    return false;
  }
  sBluedroidFunctions.bt_is_enabled = (int (*)())dlsym(handle, "bt_is_enabled");
  if(sBluedroidFunctions.bt_is_enabled == NULL) {
    NS_ERROR("Failed to attach bt_is_enabled function");
    return false;
  }
  sBluedroidFunctions.initialized = true;
  return true;
}

class ToggleBtResultTask : public nsRunnable
{
public:
  ToggleBtResultTask(nsRefPtr<BluetoothAdapter>& adapterPtr, 
                     nsCOMPtr<nsIDOMDOMRequest>& req,
                     bool enabled,
                     bool result)
    : mResult(result),
      mEnabled(enabled)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    mDOMRequest.swap(req);
    mAdapterPtr.swap(adapterPtr);
  }

  NS_IMETHOD Run() 
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Update bt power status to BluetoothAdapter only if loading bluetooth 
    // firmware succeeds.
    if (mResult) {
      mAdapterPtr->SetEnabledInternal(mEnabled);
      mAdapterPtr->SetupBluetooth();
    }

    FireEnabled(mResult, mDOMRequest);

    //mAdapterPtr must be null before returning to prevent the background 
    //thread from racing to release it during the destruction of this runnable.
    mAdapterPtr = nsnull;
    mDOMRequest = nsnull;

    return NS_OK;
  }

private:
  nsRefPtr<BluetoothAdapter> mAdapterPtr;
  nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
  bool mEnabled;
  bool mResult;
};

class ToggleBtTask : public nsRunnable
{
public:
  ToggleBtTask(bool enabled, nsIDOMDOMRequest* req, BluetoothAdapter* adapterPtr)
    : mEnabled(enabled),
      mDOMRequest(req),
      mAdapterPtr(adapterPtr) 
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() 
  {
    MOZ_ASSERT(!NS_IsMainThread());

    bool result;

#ifdef MOZ_WIDGET_GONK
    // Platform specific check for gonk until object is divided in
    // different implementations per platform. Linux doesn't require
    // bluetooth firmware loading, but code should work otherwise.
    if(!EnsureBluetoothInit()) {
      NS_ERROR("Failed to load bluedroid library.\n");
      return NS_ERROR_FAILURE;
    }

    // return 1 if it's enabled, 0 if it's disabled, and -1 on error
    int isEnabled = sBluedroidFunctions.bt_is_enabled();

    if ((isEnabled == 1 && mEnabled) || (isEnabled == 0 && !mEnabled)) {
      result = true;
    } else if (isEnabled < 0) {
      result = false;
    } else if (mEnabled) {
      result = (sBluedroidFunctions.bt_enable() == 0) ? true : false;
    } else {
      result = (sBluedroidFunctions.bt_disable() == 0) ? true : false;
    }
#else
    result = true;
    NS_WARNING("No bluetooth support in this build configuration, faking a success event instead");
#endif

    // Create a result thread and pass it to Main Thread, 
    nsCOMPtr<nsIRunnable> resultRunnable = new ToggleBtResultTask(mAdapterPtr, mDOMRequest, mEnabled, result);

    if (NS_FAILED(NS_DispatchToMainThread(resultRunnable))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }

    return NS_OK;
  }

private:
  bool mEnabled;
  nsRefPtr<BluetoothAdapter> mAdapterPtr;
  nsCOMPtr<nsIDOMDOMRequest> mDOMRequest;
};

DOMCI_DATA(BluetoothAdapter, BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothAdapter)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothAdapter, 
                                                  nsDOMEventTargetHelper)
NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(enabled)
NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(devicefound)
NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(devicecreated)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter, 
                                                nsDOMEventTargetHelper)
NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(enabled)
NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(devicefound)
NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(devicecreated)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothAdapter)
NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothAdapter)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothAdapter)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothAdapter, nsDOMEventTargetHelper)

BluetoothAdapter::BluetoothAdapter(nsPIDOMWindow *aWindow) 
: mEnabled(false),
  mDiscoverable(false),
  mClass(0),
  mPairable(false),
  mPairableTimeout(0),
  mDiscoverableTimeout(0),
  mDiscovering(0),
  mSocket(NULL),
  mScoSocket(NULL),
  mName(NS_LITERAL_STRING("testing"))
{
  BindToOwner(aWindow);
}

nsresult
BluetoothAdapter::RunAdapterFunction(const char* function_name) {
  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);
  reply = dbus_func_args(mAdapterPath,
                         DBUS_ADAPTER_IFACE, function_name,
                         DBUS_TYPE_INVALID);
  if (!reply) {
    if (dbus_error_is_set(&err)) {
      //LOG_AND_FREE_DBUS_ERROR(&err);
      LOG("Error!\n");
    } else
      LOG("DBus reply is NULL in function %s\n", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
BluetoothAdapter::SetupBluetooth()
{
  GetAdapterPath();
  GetProperties();

  nsresult rv = SetupBluetoothAgents();

  Listen(BluetoothUtils::NextAvailableChannel());

  return rv;
}

NS_IMETHODIMP
BluetoothAdapter::SetEnabled(bool aEnabled, nsIDOMDOMRequest** aDomRequest)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService("@mozilla.org/dom/dom-request-service;1");

  if (!rs) {
    NS_ERROR("No DOMRequest Service!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDOMRequest> request;
  nsresult rv = rs->CreateRequest(GetOwner(), getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!mToggleBtThread) {
    mToggleBtThread = new LazyIdleThread(15000);
  }

  nsCOMPtr<nsIRunnable> r = new ToggleBtTask(aEnabled, request, this);

  rv = mToggleBtThread->Dispatch(r, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aDomRequest);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetEnabled(bool* aEnabled)
{
  *aEnabled = mEnabled;
  return NS_OK; 
}

void
get_property(DBusMessageIter dict_entry, BluetoothProperties* properties, 
             int* ret_property_index, property_value* ret_property_value, int* ret_length)
{
  DBusMessageIter prop_value, array_value;

  int type = dbus_message_iter_get_arg_type(&dict_entry);

  if (type != DBUS_TYPE_STRING) {
    LOG("Type = %d, not DBUS_TYPE_STRING\n", type);
  } else { 
    char* property_name;
    dbus_message_iter_get_basic(&dict_entry, &property_name);
    LOG("Property Name:%s, ", property_name);

    if (!dbus_message_iter_next(&dict_entry)) {
      LOG("No next item.");
    } else {
      if (dbus_message_iter_get_arg_type(&dict_entry) == DBUS_TYPE_VARIANT) {
        // Search for the matching property and get its type
        int property_type = DBUS_TYPE_INVALID;
        int property_size = sizeof(remote_device_properties) / sizeof(BluetoothProperties);
        int i;

        for (i = 0;i < property_size;++i)
        {
          if (!strncmp(property_name, properties[i].mPropertyName, strlen(property_name)))
          {
            property_type = properties[i].mType;
            break;
          }
        }

        if (property_type == DBUS_TYPE_INVALID)
        {
          *ret_property_index = -1;
          LOG("No matching property");
        } else {
          *ret_property_index = i;
          dbus_message_iter_recurse(&dict_entry, &prop_value);

          int value, array_type;
          char* str;

          switch(property_type)
          {
            case DBUS_TYPE_STRING:
            case DBUS_TYPE_OBJECT_PATH:
              dbus_message_iter_get_basic(&prop_value, &str);
              LOG("Value : %s\n", str);
              ret_property_value->str_val = str;
              break;

            case DBUS_TYPE_UINT32:
            case DBUS_TYPE_INT16:
            case DBUS_TYPE_BOOLEAN:
              dbus_message_iter_get_basic(&prop_value, &value);
              LOG("Value : %d\n", value);
              ret_property_value->int_val = value;
              break;
              
            case DBUS_TYPE_ARRAY:
              dbus_message_iter_recurse(&prop_value, &array_value);
              array_type = dbus_message_iter_get_arg_type(&array_value);

              *ret_length = 0;
 
              if (array_type == DBUS_TYPE_OBJECT_PATH ||
                  array_type == DBUS_TYPE_STRING) {
                int len = 0;

                do {
                  ++len;
                } while (dbus_message_iter_next(&array_value));
                dbus_message_iter_recurse(&prop_value, &array_value);

                *ret_length = len;
                char** temp = (char**)malloc(sizeof(char *) * len);

                len = 0;
                do {
                  dbus_message_iter_get_basic(&array_value, &temp[len]);
                  LOG("Array Item %d: %s", len, temp[len]);

                  ++len;
                } while(dbus_message_iter_next(&array_value));

                ret_property_value->array_val = temp;
              }
              break;

            default:
              LOG("Unknown type = %d\n", property_type);
              break;
          }
        }
      } 
    }
  }
}

nsresult
BluetoothAdapter::HandleEvent(DBusMessage* msg) 
{
  if(!msg) {
    LOG("Null message, ignoring\n");
    return NS_ERROR_FAILURE;
  } 
  
  DBusError err;
  dbus_error_init(&err);

  LOG("%s: Received signal %s:%s from %s", __FUNCTION__,
      dbus_message_get_interface(msg), dbus_message_get_member(msg),
      dbus_message_get_path(msg));

  if (dbus_message_is_signal(msg,
                             DBUS_ADAPTER_IFACE,
                             "DeviceFound")) {
    char *c_address;
    DBusMessageIter iter, dict_entry, dict;

    if (dbus_message_iter_init(msg, &iter)) {
      dbus_message_iter_get_basic(&iter, &c_address);
      LOG("BD Address : %s\n", c_address);

      if (!dbus_message_iter_next(&iter)) {
        LOG("Process to next failed\n");
        return NS_ERROR_FAILURE;
      }

      int type = dbus_message_iter_get_arg_type(&iter);
      if (type != DBUS_TYPE_ARRAY) {
        LOG("Type = %d, not DUBS_TYPE_ARRAY\n", type);
        return NS_ERROR_FAILURE;
      }

      dbus_message_iter_recurse(&iter, &dict);

      do {
        property_value prop_value;
        int prop_index, array_length;

        type = dbus_message_iter_get_arg_type(&dict);
        if (type != DBUS_TYPE_DICT_ENTRY) {
          LOG("Type = %d, not DBUS_TYPE_DICT_ENTRY\n", type);
          return NS_ERROR_FAILURE;
        }

        dbus_message_iter_recurse(&dict, &dict_entry);
        get_property(dict_entry, remote_device_properties, &prop_index, &prop_value, &array_length);
      } while (dbus_message_iter_next(&dict));

      //nsIDOMBluetoothDevice* devicePtr = new BluetoothDevice(c_address, GetObjectPath(c_address));
      //FireDeviceFound(devicePtr);
    }
  } else if (dbus_message_is_signal(msg,
                                    DBUS_ADAPTER_IFACE,
                                    "DeviceDisappered")) {
    char *c_object_path;
    if (dbus_message_get_args(msg, &err, 
                              DBUS_TYPE_OBJECT_PATH, &c_object_path,
                              DBUS_TYPE_INVALID)) {
      LOG("Device address = %s has been disappeared\n", c_object_path);
    } else {
      LOG("DeviceDisappeared get args failed\n");
    }
  } else if (dbus_message_is_signal(msg,
                                    DBUS_ADAPTER_IFACE,
                                    "DeviceCreated")) {
    char *c_object_path;
    if (dbus_message_get_args(msg, &err,
                              DBUS_TYPE_OBJECT_PATH, &c_object_path,
                              DBUS_TYPE_INVALID)) {
      LOG("Device address = %s has been created\n", c_object_path);
    } else {
      LOG("DeviceCreated get args failed\n");
    }
  } else if (dbus_message_is_signal(msg,
                                    DBUS_ADAPTER_IFACE,
                                    "DeviceRemoved")) {
    char *c_object_path;
    if (dbus_message_get_args(msg, &err,
                              DBUS_TYPE_OBJECT_PATH, &c_object_path,
                              DBUS_TYPE_INVALID)) {
      LOG("Device address = %s has been removed\n", c_object_path);
    } else {
      LOG("DeviceRemoved get args failed\n");
    }
  } else if (dbus_message_is_signal(msg, 
                                    DBUS_ADAPTER_IFACE,
                                    "PropertyChanged")) {
    DBusMessageIter iter;
    char* property_name;

    if (!dbus_message_iter_init(msg, &iter)) {
      LOG("Iterator init failed.\n");
      return NS_ERROR_FAILURE;
    } else {
      dbus_message_iter_get_basic(&iter, &property_name);
      LOG("Adapter Property [%s] changed.", property_name);
    }
    
    // TODO: Need to notify JS some properties have been changed
    // GetProperties();
  } else if (dbus_message_is_signal(msg,
                                    DBUS_DEVICE_IFACE,
                                    "PropertyChanged")) {
    DBusMessageIter iter;
    char* property_name;

    if (!dbus_message_iter_init(msg, &iter)) {
      LOG("Iterator init failed.\n");
      return NS_ERROR_FAILURE;
    } else {
      dbus_message_iter_get_basic(&iter, &property_name);
      LOG("Device Property [%s] changed.", property_name);
    }

    // TODO(Eric)
    //   Need to notify JS that device property has been changed
    /*
    jobjectArray str_array = parse_remote_device_property_change(env, msg);
    if (str_array != NULL) {
      const char *remote_device_path = dbus_message_get_path(msg);
      env->CallVoidMethod(nat->me,
          method_onDevicePropertyChanged,
          env->NewStringUTF(remote_device_path),
          str_array);
    }
    */
  } else if (dbus_message_is_signal(msg,
                                    DBUS_DEVICE_IFACE,
                                    "DisconnectRequested")) {
    // const char *remote_device_path = dbus_message_get_path(msg);
    // TODO(Eric)
    //   Need to notify JS that remote device request to disconnect.
    /*
    env->CallVoidMethod(nat->me,
        method_onDeviceDisconnectRequested,
        env->NewStringUTF(remote_device_path));
    */
  }

  LOG("Event handling done\n");
  return NS_OK;
}

void BluetoothAdapter::GetAdapterPath() {
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
      return;
    }
    dbus_message_append_args(msg, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(mConnection, msg, -1, &err);

    if (!reply) {
      if (dbus_error_is_set(&err)) {
        if (dbus_error_has_name(&err,
                                "org.freedesktop.DBus.Error.ServiceUnknown")) {
          // bluetoothd is still down, retry
          //LOG_AND_FREE_DBUS_ERROR(&err);
          LOG("Service unknown\n");
          //usleep(10000);  // 10 ms
          continue;
        } else {
          // Some other error we weren't expecting
          LOG("other error\n");
          //LOG_AND_FREE_DBUS_ERROR(&err);
        }
      }
      goto failed;
    }
  }
  if (attempt == 1000) {
    LOG("timeout\n");
    //LOGE("Time out while trying to get Adapter path, is bluetoothd up ?");
    goto failed;
  }

  if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH,
                             &device_path, DBUS_TYPE_INVALID)
      || !device_path){
    if (dbus_error_is_set(&err)) {
      //LOG_AND_FREE_DBUS_ERROR(&err);
    }
    goto failed;
  }
  dbus_message_unref(msg);
  LOG("Adapter path: %s\n", device_path);
  mAdapterPath = device_path;
  return;
failed:
  dbus_message_unref(msg);
}

void 
BluetoothAdapter::GetProperties() {
  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args(mAdapterPath,
                         DBUS_ADAPTER_IFACE, "GetProperties",
                         DBUS_TYPE_INVALID);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      LOG("Error!\n");
    } else
      LOG("DBus reply is NULL in function %s\n", __FUNCTION__);
    return ;
  }

  DBusMessageIter iter, dict_entry, dict;
  int prop_index, array_length;
  property_value prop_value;

  if (dbus_message_iter_init(reply, &iter)) {
    int type = dbus_message_iter_get_arg_type(&iter);
    if (type != DBUS_TYPE_ARRAY) {
      LOG("Type = %d, not DUBS_TYPE_ARRAY\n", type);
      return;
    }

    dbus_message_iter_recurse(&iter, &dict);

    do {
      type = dbus_message_iter_get_arg_type(&dict);
      if (type != DBUS_TYPE_DICT_ENTRY) {
        LOG("Type = %d, not DBUS_TYPE_DICT_ENTRY\n", type);
        return;
      }

      dbus_message_iter_recurse(&dict, &dict_entry);
      get_property(dict_entry, adapter_properties, &prop_index, &prop_value, &array_length);

      // TODO: Not very good. Need to be refined.
      switch (prop_index)
      {
        case BT_ADAPTER_ADDRESS:
          mAddress = NS_ConvertASCIItoUTF16(prop_value.str_val);
          break;
        case BT_ADAPTER_NAME:
          mName = NS_ConvertASCIItoUTF16(prop_value.str_val);
          break;
        case BT_ADAPTER_CLASS:
          mClass = prop_value.int_val;
          break;
        case BT_ADAPTER_POWERED:
          mPower = (prop_value.int_val == 0) ? false : true;
          break;
        case BT_ADAPTER_DISCOVERABLE:
          mDiscoverable = (prop_value.int_val == 0) ? false : true;
          break;
        case BT_ADAPTER_DISCOVERABLE_TIMEOUT:
          mDiscoverableTimeout = prop_value.int_val;
          break;
        case BT_ADAPTER_PAIRABLE:
          mPairable = (prop_value.int_val == 0) ? false : true;
          break;
        case BT_ADAPTER_PAIRABLE_TIMEOUT:
          mPairableTimeout = prop_value.int_val;
          break;
        case BT_ADAPTER_DISCOVERING:
          mDiscovering = (prop_value.int_val == 0) ? false : true;
          break;
        case BT_ADAPTER_DEVICES:
          mDevices.Clear();

          for (int i = 0;i < array_length;++i)
          {
            mDevices.AppendElement(NS_ConvertASCIItoUTF16(prop_value.array_val[i]));
          }
          break;
        case BT_ADAPTER_UUIDS:
          mUuids.Clear();

          for (int i = 0;i < array_length;++i)
          {
            mUuids.AppendElement(NS_ConvertASCIItoUTF16(prop_value.array_val[i]));
          }
          break;
      }
    } while (dbus_message_iter_next(&dict));
  } else {
    LOG("Return value is wrong!\n");
  }

  dbus_message_unref(reply);
}

bool
BluetoothAdapter::SetProperty(char* propertyName, int type, void* value)
{
  DBusMessage *reply, *msg;
  DBusMessageIter iter;
  DBusError err;

   /* Initialization */
  dbus_error_init(&err);

  /* Compose the command */
  msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, mAdapterPath,
      DBUS_ADAPTER_IFACE, "SetProperty");

  if (msg == NULL) {
    LOG("SetProperty : Error on creating new method call msg");
    return false;
  }

  dbus_message_append_args(msg, DBUS_TYPE_STRING, &propertyName, DBUS_TYPE_INVALID);
  dbus_message_iter_init_append(msg, &iter);
  append_variant(&iter, type, value);

  /* Send the command. */
  reply = dbus_connection_send_with_reply_and_block(mConnection, msg, -1, &err);
  dbus_message_unref(msg);

  if (!reply || dbus_error_is_set(&err)) {
    LOG("SetProperty : Send SetProperty Command error");
    return false;
  }

  return true;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscoverable(bool* aDiscoverable)
{
  *aDiscoverable = mDiscoverable;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetDiscoverable(const bool aDiscoverable)
{
  if(aDiscoverable == mDiscoverable) return NS_OK;

  int value = aDiscoverable ? 1 : 0;

  if (!SetProperty("Discoverable", DBUS_TYPE_BOOLEAN, (void*)&value)) {
    return NS_ERROR_FAILURE;
  }

  mDiscoverable = aDiscoverable;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetAddress(nsAString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetClass(PRUint32* aClass)
{
  *aClass = mClass;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetPower(bool* aPower)
{
  *aPower = mPower;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetName(const nsAString& aName)
{
  if (mName.Equals(aName)) return NS_OK;

  const char* asciiName = ToNewCString(aName);

  if (!SetProperty("Name", DBUS_TYPE_STRING, (void*)&asciiName)) {
    return NS_ERROR_FAILURE;
  }

  mName = aName;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetPairable(bool* aPairable)
{
  *aPairable = mPairable;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetPairable(const bool aPairable)
{
  if(aPairable == mPairable) return NS_OK;

  int value = aPairable ? 1 : 0;

  if (!SetProperty("Pairable", DBUS_TYPE_BOOLEAN, (void*)&value)) {
    return NS_ERROR_FAILURE;
  }

  mPairable = aPairable;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetPower(const bool aPower)
{
  if(mPower == aPower) return NS_OK;
  // setProperty(mAdapterProxy,"Pairable", aPairable);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetPairabletimeout(PRUint32* aPairableTimeout)
{
  *aPairableTimeout = mPairableTimeout;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetPairabletimeout(const PRUint32 aPairableTimeout)
{
  if(aPairableTimeout == mPairableTimeout) return NS_OK;

  if (!SetProperty("PairableTimeout", DBUS_TYPE_UINT32, (void*)&aPairableTimeout)) {
    return NS_ERROR_FAILURE;
  }

  mPairableTimeout = aPairableTimeout;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscoverabletimeout(PRUint32* aDiscoverableTimeout)
{
  *aDiscoverableTimeout = mDiscoverableTimeout;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetDiscoverabletimeout(const PRUint32 aDiscoverableTimeout)
{
  if(aDiscoverableTimeout == mDiscoverableTimeout) return NS_OK;

  if (!SetProperty("DiscoverableTimeout", DBUS_TYPE_UINT32, (void*)&aDiscoverableTimeout)) {
    return NS_ERROR_FAILURE;
  }

  mDiscoverableTimeout = aDiscoverableTimeout;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscovering(bool* aDiscovering)
{
  *aDiscovering = mDiscovering;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDevices(JSContext* aCx, jsval* aDevices)
{
  PRUint32 length = mDevices.Length();

  if (length == 0) {
    *aDevices = JSVAL_NULL;
    return NS_OK;
  }

  jsval* devices = new jsval[length];

  for (PRUint32 i = 0; i < length; ++i) {
    devices[i].setString(JS_NewUCStringCopyN(aCx, 
                                             static_cast<const jschar*>(mDevices[i].get()),
                                             mDevices[i].Length()));
  }

  aDevices->setObjectOrNull(JS_NewArrayObject(aCx, length, devices));
  NS_ENSURE_TRUE(aDevices->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetUuids(JSContext* aCx, jsval* aUuids)
{
  PRUint32 length = mUuids.Length();

  if (length == 0) {
    *aUuids = JSVAL_NULL;
    return NS_OK;
  }

  jsval* uuids = new jsval[length];

  for (PRUint32 i = 0; i < length; ++i) {
    uuids[i].setString(JS_NewUCStringCopyN(aCx,
                                           static_cast<const jschar*>(mUuids[i].get()),
                                           mUuids[i].Length()));
  }

  aUuids->setObjectOrNull(JS_NewArrayObject(aCx, length, uuids));
  NS_ENSURE_TRUE(aUuids->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}


// nsresult
// BluetoothAdapter::firePropertyChanged()
// {
//   nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
//   nsresult rv = event->InitEvent(NS_STRING_LITERAL("propertychanged"), false, false);
//   NS_ENSURE_SUCCESS(rv, rv);
  
//   bool dummy;
//   rv = DispatchEvent(event, &dummy);
//   NS_ENSURE_SUCCESS(rv, rv);

//   return NS_OK;
// }

// /* static */ void
// BluetoothAdapter::PropertyChanged(DBusGProxy* aProxy, const gchar* aObjectPath,
//                                   GValue *value, BluetoothAdapter* aListener)
// {
//   aListener->UpdateProperties();
// }

// void
// BluetoothAdapter::UpdateProperties() {
//   AdapterPropertyChangeNotification p;
//   mObservers.Broadcast(p);
// }

NS_IMETHODIMP
BluetoothAdapter::StartDiscovery() {
  return RunAdapterFunction("StartDiscovery");
}

NS_IMETHODIMP
BluetoothAdapter::StopDiscovery() {
  return RunAdapterFunction("StopDiscovery");
}

nsresult
BluetoothAdapter::FireDeviceFound(nsIDOMBluetoothDevice* aDevice)
{
  nsRefPtr<nsDOMEvent> event = new BluetoothEvent(nsnull, nsnull);
  static_cast<BluetoothEvent*>(event.get())->SetDeviceInternal(aDevice);

  nsresult rv = event->InitEvent(NS_LITERAL_STRING("devicefound"), false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void 
asyncCreateDeviceCallback(DBusMessage *msg, void *data, void* n)
{
  DBusError err;
  dbus_error_init(&err);

  if (dbus_set_error_from_message(&err, msg)) {
    if (dbus_error_has_name(&err, "org.blueZ.Error.AlreadyExists")) {
      LOG("Device already exists\n");
    } else {
      LOG("Creating device failed\n");
    }
  } else {
    LOG("Device has been created");
  }
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

NS_IMETHODIMP
BluetoothAdapter::BluezCreateDevice(const char* aAddress)
{
  bool ret = dbus_func_args_async(5000,
        asyncCreateDeviceCallback,
        (void*)aAddress,
        mAdapterPath,
        DBUS_ADAPTER_IFACE,
        "CreateDevice",
        DBUS_TYPE_STRING, &aAddress,
        DBUS_TYPE_INVALID);

  return ret ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BluetoothAdapter::Unpair(const nsAString& aObjectPath)
{
  const char* asciiObjectPath = NS_LossyConvertUTF16toASCII(aObjectPath).get();

  bool ret = dbus_func_args_async(5000,
        nsnull,
        nsnull,
        mAdapterPath,
        DBUS_ADAPTER_IFACE,
        "RemoveDevice",
        DBUS_TYPE_OBJECT_PATH, &asciiObjectPath,
        DBUS_TYPE_INVALID);

  return ret ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BluetoothAdapter::BluezCancelDeviceCreation(const char* aAddress)
{
  bool ret = dbus_func_args_timeout(5000,
        mAdapterPath,
        DBUS_ADAPTER_IFACE,
        "CancelDeviceCreation",
        DBUS_TYPE_STRING, &aAddress,
        DBUS_TYPE_INVALID);

  return ret ? NS_OK : NS_ERROR_FAILURE;
}

extern DBusHandlerResult agent_event_filter(DBusConnection *conn,
                                            DBusMessage *msg, void *data);
static const DBusObjectPathVTable agent_vtable = {
                    NULL, agent_event_filter, NULL, NULL, NULL, NULL };

NS_IMETHODIMP
BluetoothAdapter::BluezRegisterAgent(const char * agent_path, const char * capabilities) {
    DBusMessage *msg, *reply;
    DBusError err;
    int oob = 0;

    if (!dbus_connection_register_object_path(mConnection, agent_path,
                                              &agent_vtable, NULL)) {
      LOG("%s: Can't register object path %s for agent!",
           __FUNCTION__, agent_path);
      return NS_ERROR_FAILURE;
    }

    msg = dbus_message_new_method_call("org.bluez", mAdapterPath,
                                       "org.bluez.Adapter", "RegisterAgent");
    if (!msg) {
      LOG("%s: Can't allocate new method call for agent!",
           __FUNCTION__);
      return NS_ERROR_FAILURE;
    }

    dbus_message_append_args(msg, 
                             DBUS_TYPE_OBJECT_PATH, &agent_path,
                             DBUS_TYPE_STRING, &capabilities,
                             DBUS_TYPE_BOOLEAN, &oob,
                             DBUS_TYPE_INVALID);

    LOG("Prepare to RegisterAgent");
 
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(mConnection, msg, -1, &err);
    dbus_message_unref(msg);

    if (!reply) {
      LOG("%s: Can't register agent!", __FUNCTION__);
      if (dbus_error_is_set(&err)) {
        //LOG_AND_FREE_DBUS_ERROR(&err);
      }
      return NS_ERROR_FAILURE;
    }

    dbus_message_unref(reply);
    dbus_connection_flush(mConnection);

    return NS_OK;
}

nsresult
BluetoothAdapter::SetupBluetoothAgents()
{
  const char *capabilities = "DisplayYesNo";
  const char *device_agent_path = "/B2G/bluetooth/remote_device_agent";

  LOG("Setup Bluetooth Agents");

  nsresult rv = BluezRegisterAgent("/B2G/bluetooth/agent", capabilities);
  NS_ENSURE_SUCCESS(rv, rv);

  // Register agent for remote devices.
  static const DBusObjectPathVTable agent_vtable = {
    NULL, agent_event_filter, NULL, NULL, NULL, NULL };

  if (!dbus_connection_register_object_path(mConnection, device_agent_path,
        &agent_vtable, NULL)) {
    LOG("%s: Can't register object path %s for remote device agent!",
        __FUNCTION__, device_agent_path);
    return NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
BluetoothAdapter::Pair(const nsAString& aAddress)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  const char *capabilities = "DisplayYesNo";
  const char *device_agent_path = "/B2G/bluetooth/remote_device_agent";

  char* backupAddress = new char[strlen(asciiAddress)];
  memcpy(backupAddress, asciiAddress, strlen(asciiAddress));

  // Then send CreatePairedDevice, it will register a temp device agent then 
  // unregister it after pairing process is over
  bool ret = dbus_func_args_async(10000,
      asyncCreatePairedDeviceCallback , // callback
      (void*)backupAddress,
      mAdapterPath,
      DBUS_ADAPTER_IFACE,
      "CreatePairedDevice",
      DBUS_TYPE_STRING, &asciiAddress,
      DBUS_TYPE_OBJECT_PATH, &device_agent_path,
      DBUS_TYPE_STRING, &capabilities,
      DBUS_TYPE_INVALID);

  return ret ? NS_OK : NS_ERROR_FAILURE;
}

int
BluetoothAdapter::AddServiceRecord(const char* serviceName, 
                                   unsigned long long uuidMsb, 
                                   unsigned long long uuidLsb,
                                   int channel)
{
  DBusMessage *reply;
  reply = dbus_func_args(mAdapterPath,
                         DBUS_ADAPTER_IFACE, "AddRfcommServiceRecord",
                         DBUS_TYPE_STRING, &serviceName,
                         DBUS_TYPE_UINT64, &uuidMsb,
                         DBUS_TYPE_UINT64, &uuidLsb,
                         DBUS_TYPE_UINT16, &channel,
                         DBUS_TYPE_INVALID);

  return reply ? dbus_returns_uint32(reply) : -1;
}

int
BluetoothAdapter::QueryServerChannelInternal(const char* aObjectPath)
{
  // Lookup the server channel of target profile
  const char* serviceUuid = BluetoothServiceUuidStr::Handsfree;
  int attributeId = 0x0004;

  //TODO(Eric) 
  //  We should do a service match check here to ensure the availability of
  //  requested service, however now we're only testing HANDSFREE, so just 
  //  skip this step until we actually has an array of devices.
  DBusMessage *reply = dbus_func_args(aObjectPath,
                                      DBUS_DEVICE_IFACE, "GetServiceAttributeValue",
                                      DBUS_TYPE_STRING, &serviceUuid,
                                      DBUS_TYPE_UINT16, &attributeId,
                                      DBUS_TYPE_INVALID);

  int channel = -1;

  if (reply) {
    channel = dbus_returns_int32(reply);
  }

  LOG("Handsfree: RFCOMM Server channel [%d]", channel);

  return channel;
}


NS_IMETHODIMP
BluetoothAdapter::QueryServerChannel(const nsAString& aObjectPath, PRInt32* aRetChannel)
{
  // Lookup the server channel of target profile
  const char* asciiObjectPath = NS_LossyConvertUTF16toASCII(aObjectPath).get();
  const char* serviceUuid = BluetoothServiceUuidStr::Handsfree;
  int attributeId = 0x0004;

  //TODO(Eric) 
  //  We should do a service match check here to ensure the availability of
  //  requested service, however now we're only testing HANDSFREE, so just 
  //  skip this step until we actually has an array of devices.
  DBusMessage *reply = dbus_func_args(asciiObjectPath,
                                      DBUS_DEVICE_IFACE, "GetServiceAttributeValue",
                                      DBUS_TYPE_STRING, &serviceUuid,
                                      DBUS_TYPE_UINT16, &attributeId,
                                      DBUS_TYPE_INVALID);

  int channel = -1;

  if (reply) {
    channel = dbus_returns_int32(reply);
  }

  *aRetChannel = channel;
    
  LOG("Handsfree: RFCOMM Server channel [%d]", channel);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Listen(PRInt32 channel)
{
  if (AddServiceRecord("Voice gateway",
                       BluetoothServiceUuid::BaseMSB + BluetoothServiceUuid::HandsfreeAG,
                       BluetoothServiceUuid::BaseLSB,
                       channel) == -1) {
    LOG("Adding service record failed");
    return NS_ERROR_FAILURE;
  }

  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket();
  }

  mSocket->Listen(channel);
  mSocket->Accept();

  return NS_OK;
}

const char*
BluetoothAdapter::GetObjectPath(const char* aAddress)
{
  DBusMessage *reply;
  char* retObjectPath = "";

  reply = dbus_func_args(mAdapterPath,
                         DBUS_ADAPTER_IFACE, "FindDevice",
                         DBUS_TYPE_STRING, &aAddress,
                         DBUS_TYPE_INVALID);

  DBusError err;

  dbus_error_init(&err);
  if (!dbus_message_get_args(reply, &err,
                             DBUS_TYPE_OBJECT_PATH, &retObjectPath,
                             DBUS_TYPE_INVALID)) {
    //LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
  }

  dbus_message_unref(reply);

  return retObjectPath;
}

NS_IMETHODIMP
BluetoothAdapter::ConnectHeadset(const nsAString& aAddress)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  const char* objectPath = GetObjectPath(asciiAddress);

  if (!strcmp(objectPath, "")) {
    LOG("Cannot find target device, please check if this device has already been created.");
    return NS_ERROR_FAILURE;
  }

  int channel = QueryServerChannelInternal(objectPath);

  if (channel == -1) {
    LOG("Cannot find Handsfree channel");
    return NS_ERROR_FAILURE;
  }

  if (AddServiceRecord("Voice gateway",
                       BluetoothServiceUuid::BaseMSB + BluetoothServiceUuid::HandsfreeAG,
                       BluetoothServiceUuid::BaseLSB,
                       channel) == -1) {
    LOG("Adding service record failed");
    return NS_ERROR_FAILURE;
  }

  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket();
  }

  mSocket->Connect(channel, asciiAddress);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Connect(PRInt32 channel, const nsAString& aAddress)
{
  if (mSocket == NULL || !mSocket->Available()) {
    mSocket = new BluetoothSocket();
  }

  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  mSocket->Connect(channel, asciiAddress);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Disconnect()
{
  if (mScoSocket != NULL) {
    mScoSocket->Disconnect();
  }

  if (mSocket != NULL) {
    mSocket->Disconnect();
  }

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::ConnectSco(const nsAString& aAddress)
{
  if (mScoSocket == NULL || !mScoSocket->Available()) {
    mScoSocket = new BluetoothScoSocket();
  }

  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  mScoSocket->Connect(asciiAddress);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetAudioRoute(PRInt32 aRoute)
{
  mozilla::dom::gonk::AudioManager::SetAudioRoute(aRoute);

  LOG("Set Audio Route to BluetoothSco");

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::CheckAdapter()
{
  GetProperties();
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::CheckDevice(const nsAString& aObjectPath)
{
  const char* asciiObjectPath = NS_LossyConvertUTF16toASCII(aObjectPath).get();

  DBusMessage *reply;
  DBusError err;
  dbus_error_init(&err);

  reply = dbus_func_args_timeout(-1, asciiObjectPath,
                                 DBUS_DEVICE_IFACE, "GetProperties",
                                 DBUS_TYPE_INVALID);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      //LOG_AND_FREE_DBUS_ERROR(&err);
      LOG("ERROR!!!!!!");
    } else
      LOG("DBus reply is NULL in function %s", __FUNCTION__);

    return NS_ERROR_FAILURE;
  }

  DBusMessageIter iter, dict_entry, dict;
  if (dbus_message_iter_init(reply, &iter)) {
    int type = dbus_message_iter_get_arg_type(&iter);
    if (type != DBUS_TYPE_ARRAY) {
      LOG("Type = %d, not DUBS_TYPE_ARRAY\n", type);
      return NS_ERROR_FAILURE;
    }

    dbus_message_iter_recurse(&iter, &dict);

    do {
      property_value prop_value;
      int prop_index, array_length;

      type = dbus_message_iter_get_arg_type(&dict);
      if (type != DBUS_TYPE_DICT_ENTRY) {
        LOG("Type = %d, not DBUS_TYPE_DICT_ENTRY\n", type);
        return NS_ERROR_FAILURE;
      }

      dbus_message_iter_recurse(&dict, &dict_entry);
      get_property(dict_entry, remote_device_properties, &prop_index, &prop_value, &array_length);

      // TODO: Not very good. Need to be refined.
      /*
      switch (prop_index)
      {
        case BT_DEVICE_ADDRESS:
        case BT_DEVICE_NAME:
        case BT_DEVICE_PAIRED:
        case BT_DEVICE_CONNECTED:
        case BT_DEVICE_ADAPTER:
          break;
      }
      */
    } while (dbus_message_iter_next(&dict));
  }

  dbus_message_unref(reply);

  return NS_OK;
}

nsresult
BluetoothAdapter::GetDevice(const nsAString& aAddress, nsIDOMBluetoothDevice** aDevice)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  nsCOMPtr<nsIDOMBluetoothDevice> ptr = new BluetoothDevice(asciiAddress, GetObjectPath(asciiAddress));

  NS_ADDREF(*aDevice = ptr);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BluetoothAdapter, propertychanged)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicefound)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicedisappeared)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicecreated)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, deviceremoved)
