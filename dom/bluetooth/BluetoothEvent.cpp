#include "BluetoothEvent.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMBluetoothDevice.h"

DOMCI_DATA(BluetoothEvent, mozilla::dom::bluetooth::BluetoothEvent)

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDevice)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDevice)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBluetoothEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMBluetoothEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(BluetoothEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(BluetoothEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(BluetoothEvent, nsDOMEvent)

NS_IMETHODIMP
BluetoothEvent::GetDevice(nsIDOMBluetoothDevice** aDevice)
{
  NS_IF_ADDREF(*aDevice = mDevice);
  return NS_OK;
}

void
BluetoothEvent::SetDeviceInternal(nsIDOMBluetoothDevice* aDevice)
{
  mDevice = aDevice;
}

nsresult
NS_NewDOMBluetoothEvent(nsIDOMEvent** aInstancePtrResult,
                  nsPresContext* aPresContext,
                  nsEvent* aEvent)
{
    return CallQueryInterface(new BluetoothEvent(aPresContext, aEvent),
                              aInstancePtrResult);
}

