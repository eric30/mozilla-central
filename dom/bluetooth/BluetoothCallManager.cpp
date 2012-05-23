#include "BluetoothCallManager.h"
#include "BluetoothCommon.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "BluetoothCallManager", args)
#else
#define LOG(args...) printf(args); printf("\n");
#endif

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothRILTelephonyCallback : public nsIRILTelephonyCallback
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRILTELEPHONYCALLBACK

    BluetoothRILTelephonyCallback() {} 
};

NS_IMPL_ISUPPORTS1(BluetoothRILTelephonyCallback, nsIRILTelephonyCallback)

NS_IMETHODIMP 
BluetoothRILTelephonyCallback::CallStateChanged(PRUint32 aCallIndex, 
                                                PRUint16 aCallState,
                                                const nsAString& aNumber)
{
  const char* number = NS_LossyConvertUTF16toASCII(aNumber).get();
  LOG("Get call state changed: index=%d, state=%d, number=%s", aCallIndex, aCallState, number);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothRILTelephonyCallback::EnumerateCallState(PRUint32 aCallIndex,
                                                  PRUint16 aCallState,
                                                  const nsAString_internal& aNumber, 
                                                  bool aIsActive, 
                                                  bool* aResult)
{
  *aResult = true;
  return NS_OK;
}

BluetoothCallManager::BluetoothCallManager()
{
  mRIL = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  mRILTelephonyCallback = new BluetoothRILTelephonyCallback();

  nsresult rv = mRIL->EnumerateCalls(mRILTelephonyCallback);
  rv = mRIL->RegisterTelephonyCallback(mRILTelephonyCallback);
}

void
BluetoothCallManager::HangUp()
{
  mRIL->HangUp(0);
}

void
BluetoothCallManager::Answer()
{
  mRIL->AnswerCall(0);
}

END_BLUETOOTH_NAMESPACE
