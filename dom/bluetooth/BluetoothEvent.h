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

  BluetoothEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)
  { }

  BluetoothEvent()
    : nsDOMEvent(nsnull, nsnull)
  { }

  void SetDeviceAddressInternal(const nsACString&);

protected:
  nsCString mAdapterAddress;
  nsCString mDeviceAddress;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothEvent_h
