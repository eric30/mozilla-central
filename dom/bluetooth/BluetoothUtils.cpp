#include "BluetoothUtils.h"

USING_BLUETOOTH_NAMESPACE

int BluetoothUtils::mChannel = 2;

int
BluetoothUtils::NextAvailableChannel()
{
  return BluetoothUtils::mChannel++;
}
