/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothAdapter.h"

#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"
#include <dlfcn.h>
#include "dbus/dbus.h"

#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"

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
    printf("This isn't a dictionary!\n");
    return false;
  }
  dbus_message_iter_recurse(&iter, &dict);
  do {
    len = 0;
    if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_DICT_ENTRY)
    {
      printf("Not a dict entry!\n");
      return false;
    }            
    dbus_message_iter_recurse(&dict, &dict_entry);
    DBusMessageIter prop_val, array_val_iter;
    char *property = NULL;
    uint32_t array_type;
    char *str_val;
    int i, j, type, int_val;

    if (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_STRING) {
      printf("Iter not a string!\n");
      return false;
    }
    dbus_message_iter_get_basic(&dict_entry, &property);
    printf("Property: %s\n", property);
    if(strcmp(property, name) != 0) continue;
    if (!dbus_message_iter_next(&dict_entry))
    {
      printf("No next!\n");
      return false;
    }
    if (dbus_message_iter_get_arg_type(&dict_entry) != DBUS_TYPE_VARIANT)
    {
      printf("Iter not a variant!\n");
      return false;
    }
    dbus_message_iter_recurse(&dict_entry, &prop_val);
    // type = properties[*prop_index].type;
    if (dbus_message_iter_get_arg_type(&prop_val) != expected_type) {
      //   LOGE("Property type mismatch in get_property: %d, expected:%d, index:%d",
      //        dbus_message_dict_entry_get_arg_type(&prop_val), type, *prop_index);
      printf("Not the type we expect!\n");
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothAdapter, 
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(enabled)
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
  mName(NS_LITERAL_STRING("testing"))
{
  BindToOwner(aWindow);
}

nsresult
BluetoothAdapter::RunAdapterFunction(const char* function_name) {
  DBusMessage *msg, *reply;
  DBusError err;
  dbus_error_init(&err);
  GetAdapterPath();
  reply = dbus_func_args(mAdapterPath,
                         DBUS_ADAPTER_IFACE, function_name,
                         DBUS_TYPE_INVALID);
  if (!reply) {
    if (dbus_error_is_set(&err)) {
      //LOG_AND_FREE_DBUS_ERROR(&err);
      printf("Error!\n");
    } else
      printf("DBus reply is NULL in function %s\n", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;

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


nsresult
BluetoothAdapter::HandleEvent(DBusMessage* msg) {
  if(!msg) {
    printf("Null message, ignoring\n");
  }
  printf("Handling an event!\n");
  // if (dbus_message_is_signal(msg,
  //                            "org.bluez.Adapter",
  //                            "DeviceFound")) {
  //     char *c_address;
  //     DBusMessageIter iter;
  //     jobjectArray str_array = NULL;
  //     if (dbus_message_iter_init(msg, &iter)) {
  //         dbus_message_iter_get_basic(&iter, &c_address);
  //         if (dbus_message_iter_next(&iter))
  //             str_array =
  //                 parse_remote_device_properties(env, &iter);
  //     }
  //     if (str_array != NULL) {
  //         env->CallVoidMethod(dbt->me,
  //                             method_onDeviceFound,
  //                             env->NewStringUTF(c_address),
  //                             str_array);
  //     } else
  //         LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
  //     goto success;
  // } else if (dbus_message_is_signal(msg,
  //                                  "org.bluez.Adapter",
  //                                  "DeviceDisappeared")) {
  //     char *c_address;
  //     if (dbus_message_get_args(msg, &err,
  //                               DBUS_TYPE_STRING, &c_address,
  //                               DBUS_TYPE_INVALID)) {
  //         LOGV("... address = %s", c_address);
  //         env->CallVoidMethod(dbt->me, method_onDeviceDisappeared,
  //                             env->NewStringUTF(c_address));
  //     } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
  //     goto success;
  // } else if (dbus_message_is_signal(msg,
  //                                  "org.bluez.Adapter",
  //                                  "DeviceCreated")) {
  //     char *c_object_path;
  //     if (dbus_message_get_args(msg, &err,
  //                               DBUS_TYPE_OBJECT_PATH, &c_object_path,
  //                               DBUS_TYPE_INVALID)) {
  //         LOGV("... address = %s", c_object_path);
  //         env->CallVoidMethod(dbt->me,
  //                             method_onDeviceCreated,
  //                             env->NewStringUTF(c_object_path));
  //     } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
  //     goto success;
  // } else if (dbus_message_is_signal(msg,
  //                                  "org.bluez.Adapter",
  //                                  "DeviceRemoved")) {
  //     char *c_object_path;
  //     if (dbus_message_get_args(msg, &err,
  //                              DBUS_TYPE_OBJECT_PATH, &c_object_path,
  //                              DBUS_TYPE_INVALID)) {
  //        LOGV("... Object Path = %s", c_object_path);
  //        env->CallVoidMethod(dbt->me,
  //                            method_onDeviceRemoved,
  //                            env->NewStringUTF(c_object_path));
  //     } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
  //     goto success;
  // } else
  if (dbus_message_is_signal(msg,
                             "org.bluez.Adapter",
                             "PropertyChanged")) {
    // jobjectArray str_array = parse_adapter_property_change(env, msg);
    // if (str_array != NULL) {
    //   /* Check if bluetoothd has (re)started, if so update the path. */
    //   jstring property =(jstring) env->GetObjectArrayElement(str_array, 0);
    //   const char *c_property = env->GetStringUTFChars(property, NULL);
    //   if (!strncmp(c_property, "Powered", strlen("Powered"))) {
    //     jstring value =
    //       (jstring) env->GetObjectArrayElement(str_array, 1);
    //     const char *c_value = env->GetStringUTFChars(value, NULL);
    //     if (!strncmp(c_value, "true", strlen("true")))
    //       dbt->adapter = get_adapter_path(dbt->conn);
    //     env->ReleaseStringUTFChars(value, c_value);
    //   }
    //   env->ReleaseStringUTFChars(property, c_property);

    //   env->CallVoidMethod(dbt->me,
    //                       method_onPropertyChanged,
    //                       str_array);
    // } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
    printf("Got a property changed message!\n");
    GetProperties();
  }
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
      printf("%s: Can't allocate new method call for get_adapter_path!",
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
          printf("Service unknown\n");
          usleep(10000);  // 10 ms
          continue;
        } else {
          // Some other error we weren't expecting
          printf("other error\n");
          //LOG_AND_FREE_DBUS_ERROR(&err);
        }
      }
      goto failed;
    }
  }
  if (attempt == 1000) {
    printf("timeout\n");
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
  printf("Adapter path: %s\n", device_path);
  mAdapterPath = device_path;
  return;
failed:
  dbus_message_unref(msg);
}

void 
BluetoothAdapter::GetProperties() {

  DBusMessage *msg, *reply;
  DBusError err;
  dbus_error_init(&err);
  GetAdapterPath();
  reply = dbus_func_args(mAdapterPath,
                         DBUS_ADAPTER_IFACE, "GetProperties",
                         DBUS_TYPE_INVALID);
  if (!reply) {
    if (dbus_error_is_set(&err)) {
      //LOG_AND_FREE_DBUS_ERROR(&err);
      printf("Error!\n");
    } else
      printf("DBus reply is NULL in function %s\n", __FUNCTION__);
    return ;
  }
  
  DBusMessageIter iter;
  //jobjectArray str_array = NULL;
  if (!dbus_message_iter_init(reply, &iter)) {
    printf("Return value is wrong!\n");
    dbus_message_unref(reply);
    return;
  }
  //str_array = parse_adapter_properties(env, &iter);
  const char* n;
  GetDBusDictValue<const char*>(iter, "Name", DBUS_TYPE_STRING, n);
  mName = NS_ConvertASCIItoUTF16(n);
  printf("name! %s\n", n);
  dbus_message_unref(reply);
  printf("Adapter properties got\n");
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
  if(mDiscoverable == aDiscoverable) return NS_OK;
  // setProperty(mAdapterProxy,"Discoverable", aDiscoverable);  
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
  if(aName == mName) return NS_OK;
  // setProperty(mAdapterProxy,"Name", aName);
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
  // setProperty(mAdapterProxy,"Pairable", aPairable);
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
  // setProperty(mAdapterProxy,"PairableTimeout", aPairableTimeout);
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
  // setProperty(mAdapterProxy,"DiscoverableTimeout", aDiscoverableTimeout);
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscovering(bool* aDiscovering)
{
  *aDiscovering = mDiscovering;
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

NS_IMPL_EVENT_HANDLER(BluetoothAdapter, propertychanged)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicefound)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicedisappeared)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicecreated)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, deviceremoved)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, powered)

