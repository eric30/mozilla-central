/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothAdapter.h"
#include "BluetoothDevice.h"
#include "BluetoothFirmware.h"
#include "BluetoothService.h"
#include "BluetoothEventHandler.h"
#include "BluetoothEvent.h"

//For test
#include "BluetoothHfpManager.h"
#include "BluetoothObexClient.h"
#include "BluetoothObexServer.h"
#include "BluetoothOppManager.h"
#include "BluetoothFtpManager.h"
#include "BluetoothServiceUuid.h"
#include "BluetoothServiceUuidHelper.h"
#include "BluetoothSocket.h"
#include "ObexBase.h"

#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Util.h"
#include "dbus/dbus.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/uio.h>


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

        if (mEnabled) {
          mAdapterPtr->Setup();
        } else {
          mAdapterPtr->TearDown();
        }
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
      int isEnabled = IsBtEnabled();

      if ((isEnabled == 1 && mEnabled) || (isEnabled == 0 && !mEnabled)) {
        result = true;
      } else if (isEnabled < 0) {
        result = false;
      } else if (mEnabled) {
        result = (EnableBt() == 0) ? true : false;
      } else {
        result = (DisableBt() == 0) ? true : false;
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

BluetoothAdapter::BluetoothAdapter(nsPIDOMWindow *aWindow) : mHandler(NULL)
                                                           , mHfpServiceHandle(-1)
                                                           , mHspServiceHandle(-1)
{
  BindToOwner(aWindow);
}

void
BluetoothAdapter::Setup()
{
  if (!mHandler) {
    mHandler = new BluetoothEventHandler();
  }

  mHandler->Register(this);

  // Register Bluetooth agent
  RegisterAgent();

  // Register reserved services, it will use uuid_16 to register, so that BlueZ will update
  // Major Service Class of CoD.
  // (For more information, please see bug 768781)
  int reservedServices[3];
  
  reservedServices[0] = BluetoothServiceUuidHelper::Get16BitServiceId(BluetoothServiceUuid::HandsfreeAG);
  reservedServices[1] = BluetoothServiceUuidHelper::Get16BitServiceId(BluetoothServiceUuid::HeadsetAG);
  reservedServices[2] = BluetoothServiceUuidHelper::Get16BitServiceId(BluetoothServiceUuid::ObjectPush);

  AddReservedServiceRecordsInternal(reservedServices, 3);

  // FTP is not reserved service.
  // (How to distinguish it's a reserved service or not?
  AddRfcommServiceRecordInternal("OBEX File Transfer",
                                 BluetoothServiceUuid::BaseMSB + BluetoothServiceUuid::FTP,
                                 BluetoothServiceUuid::BaseLSB,
                                 BluetoothFtpManager::DEFAULT_FTP_CHANNEL);

  // Start HFP server
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  hfp->WaitForConnect();

  // Start OBEX opp Server
  BluetoothOppManager* opp = new BluetoothOppManager();
  opp->Start();

  // Start OBEX ftp Server
  BluetoothFtpManager* ftp = new BluetoothFtpManager();
  ftp->Start();

  UpdateProperties();
}

void
BluetoothAdapter::TearDown()
{
  //Stop HFP server
  BluetoothHfpManager* hfp = BluetoothHfpManager::GetManager();
  hfp->StopWaiting();

  mHfpServiceHandle = -1;
  mHspServiceHandle = -1;

  UnregisterAgent();
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
BluetoothAdapter::StartDiscovery(bool* result)
{
  if (!mEnabled) {
    *result = false;
  } else {
    *result = StartDiscoveryInternal();
  }

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::StopDiscovery()
{
  StopDiscoveryInternal();

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetRemoteDevice(const nsAString& aAddress, nsIDOMBluetoothDevice** aDevice)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  nsCOMPtr<nsIDOMBluetoothDevice> device = new BluetoothDevice(asciiAddress);

  device.forget(aDevice);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Pair(const nsAString& aAddress)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  CreatePairedDeviceInternal(asciiAddress, 50000);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::Unpair(const nsAString& aAddress)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  RemoveDeviceInternal(GetObjectPathFromAddress(asciiAddress));

  return NS_OK;
}

// **************************************************
// ***************** Event Handler ******************
// **************************************************

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

nsresult
BluetoothAdapter::FireDeviceConnected(const char* address)
{
  nsString domDeviceAddress = NS_ConvertASCIItoUTF16(address);

  nsRefPtr<nsDOMEvent> event = new BluetoothEvent(nsnull, nsnull);
  static_cast<BluetoothEvent*>(event.get())->SetDeviceAddressInternal(domDeviceAddress);

  nsresult rv = event->InitEvent(NS_LITERAL_STRING("deviceconnected"), false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
BluetoothAdapter::FireDeviceDisconnected(const char* address)
{
  nsString domDeviceAddress = NS_ConvertASCIItoUTF16(address);

  nsRefPtr<nsDOMEvent> event = new BluetoothEvent(nsnull, nsnull);
  static_cast<BluetoothEvent*>(event.get())->SetDeviceAddressInternal(domDeviceAddress);

  nsresult rv = event->InitEvent(NS_LITERAL_STRING("devicedisconnected"), false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


void 
BluetoothAdapter::onDeviceFoundNative(const char* aDeviceAddress, std::list<const char*> aPropertyList)
{
  LOG("[DeviceFound] Address = %s", aDeviceAddress);

  BluetoothDevice* device = new BluetoothDevice(aDeviceAddress);

  while (!aPropertyList.empty()) {
    const char* name = aPropertyList.front();
    LOG("[Device Property Name] %s", name);
    aPropertyList.pop_front();

    if (!strcmp(name, "UUIDs")) {
      device->mUuids.Clear();

      int length = GetInt(aPropertyList.front());
      LOG("[Uuid Count] %d", length);

      while (length--) {
        aPropertyList.pop_front();
        const char* uuid = aPropertyList.front();
        device->mUuids.AppendElement(NS_ConvertASCIItoUTF16(uuid));

        LOG("[Device UUID value] %s", uuid);
      }
    } else if (!strcmp(name, "Services")) {
      int length = GetInt(aPropertyList.front());
      while (length--) {
        aPropertyList.pop_front();
      }
    } else {
      const char* value = aPropertyList.front();

      if (!strcmp(name, "Address")) {
        device->mAddress = NS_ConvertASCIItoUTF16(value);
      } else if (!strcmp(name, "Name")) {
        device->mName = NS_ConvertASCIItoUTF16(value);
      } else if (!strcmp(name, "Class")) {
        device->mClass = GetInt(value);
      } else if (!strcmp(name, "Paired")) {
        device->mPaired = GetBool(value);
      } else if (!strcmp(name, "Connected")) {
        device->mConnected = GetBool(value);
      }
    }

    aPropertyList.pop_front();
  }

  FireDeviceFound(device);
}

void 
BluetoothAdapter::onDeviceDisappearedNative(const char* aDeviceAddress)
{
  LOG("[DeviceDisappered] Address = %s", aDeviceAddress);
}

void 
BluetoothAdapter::onDeviceCreatedNative(const char* aDeviceObjectPath)
{
  LOG("[DeviceCreated] Object Path = %s", aDeviceObjectPath);
}

void 
BluetoothAdapter::onPropertyChangedNative(std::list<const char*> aChangedProperty)
{
  const char* name = aChangedProperty.front();
  aChangedProperty.pop_front();
  const char* value = aChangedProperty.front();

  LOG("[PropertyChanged] %s -> %s", name, value);
  
  if (mEnabled) {
    UpdateProperties();
  }
}

void 
BluetoothAdapter::onDevicePropertyChangedNative(const char* aObjectPath, std::list<const char*> aChangedProperty)
{
  const char* propertyName = aChangedProperty.front();
  aChangedProperty.pop_front();
  const char* value = aChangedProperty.front();

  LOG("[PropertyChanged] %s -> %s", propertyName, value);
  
  if (mEnabled) {
    if (!strcmp(propertyName, "Connected")) {
      const char* address = GetAddressFromObjectPath(aObjectPath);

      if (GetBool(value)) {
        FireDeviceConnected(address);
      } else {
        FireDeviceDisconnected(address);
      }
    }
  }
}

// **************************************************
// ******************** Getters *********************
// **************************************************

NS_IMETHODIMP
BluetoothAdapter::GetEnabled(bool* aEnabled)
{
  *aEnabled = mEnabled;
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
BluetoothAdapter::GetDiscovering(bool* aDiscovering)
{
  *aDiscovering = mDiscovering;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscoverable(bool* aDiscoverable)
{
  *aDiscoverable = mDiscoverable;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::GetDiscoverableTimeout(PRUint32* aDiscoverableTimeout)
{
  *aDiscoverableTimeout = mDiscoverableTimeout;
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

  JSObject* array = JS_NewArrayObject(aCx, length, nsnull);
  nsresult rv = NS_OK;

  for (PRUint32 i = 0; i < length; ++i) {
    jsval val;
    JSObject* scope = JS_GetGlobalForScopeChain(aCx);

    const char* deviceAddress = NS_LossyConvertUTF16toASCII(mDevices[i]).get();
    BluetoothDevice* device = new BluetoothDevice(deviceAddress);
    UpdateDeviceProperties(device);

    rv = nsContentUtils::WrapNative(aCx, scope, device, &val, nsnull, true);
    if (!JS_SetElement(aCx, array, i, &val)) {
      LOG("Set element error.");
      return NS_OK;
    }
  }

  *aDevices = OBJECT_TO_JSVAL(array);

  return NS_OK;
}

// **************************************************
// ******************** Setters *********************
// **************************************************

NS_IMETHODIMP
BluetoothAdapter::SetName(const nsAString& aName)
{
  if (mName.Equals(aName)) return NS_OK;

  const char* asciiName = ToNewCString(aName);

  if (!SetAdapterProperty("Name", DBUS_TYPE_STRING, (void*)&asciiName)) {
    return NS_ERROR_FAILURE;
  }

  mName = aName;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetDiscoverable(const bool aDiscoverable)
{
  if(aDiscoverable == mDiscoverable) return NS_OK;

  int value = aDiscoverable ? 1 : 0;

  if (!SetAdapterProperty("Discoverable", DBUS_TYPE_BOOLEAN, (void*)&value)) {
    return NS_ERROR_FAILURE;
  }

  mDiscoverable = aDiscoverable;

  return NS_OK;
}

NS_IMETHODIMP
BluetoothAdapter::SetDiscoverableTimeout(const PRUint32 aDiscoverableTimeout)
{
  if(aDiscoverableTimeout == mDiscoverableTimeout) return NS_OK;

  if (!SetAdapterProperty("DiscoverableTimeout", DBUS_TYPE_UINT32, (void*)&aDiscoverableTimeout)) {
    return NS_ERROR_FAILURE;
  }

  mDiscoverableTimeout = aDiscoverableTimeout;

  return NS_OK;
}

// **************************************************
// ************** Internal functions ****************
// **************************************************
void
BluetoothAdapter::UpdateProperties()
{
  std::list<const char*> propertiesStrArray = GetAdapterProperties();

  while (!propertiesStrArray.empty()) {
    const char* name = propertiesStrArray.front();
    propertiesStrArray.pop_front();

    LOG("[Property Name] %s", name);

    if (!strcmp(name, "Devices")) {
      mDevices.Clear();

      int length = GetInt(propertiesStrArray.front());

      LOG("[Length] %d", length);

      while (length--) {
        propertiesStrArray.pop_front();
        const char* deviceAddress = GetAddressFromObjectPath(propertiesStrArray.front());
        mDevices.AppendElement(NS_ConvertASCIItoUTF16(deviceAddress));

        LOG("[Property Value] %s", deviceAddress);
      }
    } else if (!strcmp(name, "UUIDs")) {
      mUuids.Clear();

      int length = GetInt(propertiesStrArray.front());
      LOG("[Length] %d", length);

      while (length--) {
        propertiesStrArray.pop_front();
        const char* uuid = propertiesStrArray.front();
        mUuids.AppendElement(NS_ConvertASCIItoUTF16(uuid));

        LOG("[Property Value] %s", uuid);
      }
    } else {
      const char* value = propertiesStrArray.front();

      LOG("[Property Value] %s", value);

      if (!strcmp("Address", name)) {
        mAddress = NS_ConvertASCIItoUTF16(value);
      } else if (!strcmp("Name", name)) {
        mName = NS_ConvertASCIItoUTF16(value);
      } else if (!strcmp("Discovering", name)) {
        mDiscovering = GetBool(value);
      } else if (!strcmp("Discoverable", name)) {
        mDiscoverable = GetBool(value);
      }
    }

    propertiesStrArray.pop_front();
  }
}

// xxx TEMP xxx , for OBEX testing

#include <pthread.h>

BluetoothSocket* testSocket = NULL;

nsresult
BluetoothAdapter::CreateObexConn(const nsAString& aAddress, PRUint32 aChannel, bool* result)
{
  const char* asciiAddress = NS_LossyConvertUTF16toASCII(aAddress).get();
  int channel = aChannel;

  ObexClient* obex = new ObexClient(asciiAddress, channel);
  if (obex->Init()) {
    obex->Connect();
  }

  *result = true;

  return NS_OK;
}

nsresult
BluetoothAdapter::DisconnectObex()
{
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicefound)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, deviceconnected)
NS_IMPL_EVENT_HANDLER(BluetoothAdapter, devicedisconnected)
