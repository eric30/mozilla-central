#include "BluetoothEvent.h"
#include "nsDOMClassInfo.h"

USING_BLUETOOTH_NAMESPACE

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(BluetoothEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(BluetoothEvent, nsDOMEvent)

DOMCI_DATA(BluetoothEvent, BluetoothEvent)

NS_IMETHODIMP
BluetoothEvent::GetDeviceAddress(nsACString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
  return NS_OK;
}

void 
BluetoothEvent::SetDeviceAddressInternal(const nsACString& aDeviceAddress)
{
  mDeviceAddress = aDeviceAddress;
}

