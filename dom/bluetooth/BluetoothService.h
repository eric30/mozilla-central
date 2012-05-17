/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#ifndef mozilla_dom_bluetooth_bluetoothservice_h__
#define mozilla_dom_bluetooth_bluetoothservice_h__

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

void InitServices();
const char* GetDefaultAdapterPath();
bool RegisterAgent();
void UnregisterAgent();
bool StartDiscoveryInternal();
void StopDiscoveryInternal();
void GetAdapterProperties();
bool SetAdapterProperty(char* propertyName, int type, void* value);
void GetDeviceProperties(const char* aObjectPath);
void CreatePairedDeviceInternal(const char* aAddress, int aTimeout);
void RemoveDeviceInternal(const char* aDeviceObjectPath);
void DiscoverServicesInternal(const char* aObjectPath, const char* aPattern);
int AddRfcommServiceRecordInternal(const char* aName,
                                   unsigned long long aUuidMsb,
                                   unsigned long long aUuidLsb,
                                   short aChannel);
bool RemoveServiceRecordInternal(int aHandle);
int GetDeviceServiceChannelInternal(const char* aObjectPath, const char* aPattern, int aAttrId);

END_BLUETOOTH_NAMESPACE
#endif
