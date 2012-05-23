#include "BluetoothCallManager.h"
#include "BluetoothCommon.h"
#include "BluetoothHfpManager.h"

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

    BluetoothRILTelephonyCallback(BluetoothHfpManager* aHfp) : mHfp(aHfp) {}

  private:
    BluetoothHfpManager* mHfp;
};

NS_IMPL_ISUPPORTS1(BluetoothRILTelephonyCallback, nsIRILTelephonyCallback)

NS_IMETHODIMP 
BluetoothRILTelephonyCallback::CallStateChanged(PRUint32 aCallIndex, 
                                                PRUint16 aCallState,
                                                const nsAString& aNumber)
{
  const char* number = NS_LossyConvertUTF16toASCII(aNumber).get();
  mHfp->CallStateChanged(aCallIndex, aCallState, number);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothRILTelephonyCallback::EnumerateCallState(PRUint32 aCallIndex,
                                                  PRUint16 aCallState,
                                                  const nsAString_internal& aNumber, 
                                                  bool aIsActive, 
                                                  bool* aResult)
{
  LOG("Enumerate Call State: call index=%d, call state=%d, active=%d", aCallIndex, aCallState, aIsActive);
  *aResult = true;
  return NS_OK;
}

BluetoothCallManager::BluetoothCallManager(BluetoothHfpManager* aHfp) : mHfp(aHfp)
{
  mRIL = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  mRILTelephonyCallback = new BluetoothRILTelephonyCallback(mHfp);
}

void
BluetoothCallManager::StartListening()
{
  nsresult rv = mRIL->EnumerateCalls(mRILTelephonyCallback);
  rv = mRIL->RegisterTelephonyCallback(mRILTelephonyCallback);
}

void
BluetoothCallManager::StopListening()
{
  nsresult rv = mRIL->UnregisterTelephonyCallback(mRILTelephonyCallback);
}

void
BluetoothCallManager::HangUp(int aCallIndex)
{
  mRIL->HangUp(aCallIndex);
}

void
BluetoothCallManager::Answer(int aCallIndex)
{
  mRIL->AnswerCall(aCallIndex);
}

void
BluetoothCallManager::Reject(int aCallIndex)
{
  mRIL->RejectCall(aCallIndex);
}

END_BLUETOOTH_NAMESPACE
