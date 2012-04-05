#ifndef mozilla_dom_bluetooth_BluetoothEvent_h
#define mozilla_dom_bluetooth_BluetoothEvent_h

#include "BluetoothCommon.h"
#include "nsIDOMBluetoothEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsDOMEvent.h"
#include "nsString.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothEvent : public nsIDOMBluetoothEvent
                     , public nsDOMEvent
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMBLUETOOTHEVENT
  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothEvent, nsDOMEvent)

  BluetoothEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)
    , mDevice(nsnull)
  { }

  BluetoothEvent()
    : nsDOMEvent(nsnull, nsnull)
    , mDevice(nsnull) 
  { }

  void SetDeviceAddressInternal(const nsACString&);
  void SetDeviceInternal(nsIDOMBluetoothDevice* aDevice);

protected:
  nsCString mAdapterAddress;
  nsCString mDeviceAddress;

private:
  nsCOMPtr<nsIDOMBluetoothDevice> mDevice;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothEvent_h
