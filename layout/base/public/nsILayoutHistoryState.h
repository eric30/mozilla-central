#ifndef _nsILayoutHistoryState_h
#define _nsILayoutHistoryState_h

#include "nsISupports.h"

#define NS_ILAYOUTHISTORYSTATE_IID_STR "306c8ca0-5f0c-11d3-a9fb-000064657374"

#define NS_ILAYOUTHISTORYSTATE_IID \
{0x306c8ca0, 0x5f0c, 0x11d3, \
{0xa9, 0xfb, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

#define NS_HISTORY_STATE_TYPE_NONE -1

class nsILayoutHistoryState : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILAYOUTHISTORYSTATE_IID)

  NS_IMETHOD AddState(PRUint32 aContentID, nsISupports* aState, PRInt32 aStateType) = 0;
  NS_IMETHOD GetState(PRUint32 aContentID, nsISupports** aState, PRInt32 aStateType) = 0;
};

nsresult
NS_NewLayoutHistoryState(nsILayoutHistoryState** aState);

#endif /* _nsILayoutHistoryState_h */