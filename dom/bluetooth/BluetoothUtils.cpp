#include "BluetoothUtils.h"

USING_BLUETOOTH_NAMESPACE

int BluetoothUtils::mChannel = 2;
static const int MAX_RFCOMM_CHANNEL = 30;

int
BluetoothUtils::NextAvailableChannel()
{
  if (BluetoothUtils::mChannel++ > MAX_RFCOMM_CHANNEL)
    BluetoothUtils::mChannel = 2;

  return BluetoothUtils::mChannel;
}
