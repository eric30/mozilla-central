/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_bluetooth_bluetoothproperties_h__
#define mozilla_dom_bluetooth_bluetoothproperties_h__

#include <dbus/dbus.h>

typedef union {
  char *str_val;
  int int_val;
  char **array_val;
} property_value;

class BluetoothProperties {
  public:
    BluetoothProperties(const char* aPropertyName, int aType) : mPropertyName(aPropertyName), 
                                                                mType(aType)
    {
    
    }

    const char* mPropertyName;
    int mType;
};

static BluetoothProperties adapter_properties[] = {
  BluetoothProperties("Address", DBUS_TYPE_STRING),
  BluetoothProperties("Name", DBUS_TYPE_STRING),
  BluetoothProperties("Class", DBUS_TYPE_UINT32),
  BluetoothProperties("Powered", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("Discoverable", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("DiscoverableTimeout", DBUS_TYPE_UINT32),
  BluetoothProperties("Pairable", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("PairableTimeout", DBUS_TYPE_UINT32),
  BluetoothProperties("Discovering", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("Devices", DBUS_TYPE_ARRAY),
  BluetoothProperties("UUIDs", DBUS_TYPE_ARRAY)
};

static BluetoothProperties remote_device_properties[] = {
  BluetoothProperties("Address", DBUS_TYPE_STRING),
  BluetoothProperties("Name", DBUS_TYPE_STRING),
  BluetoothProperties("Icon", DBUS_TYPE_STRING),
  BluetoothProperties("Class", DBUS_TYPE_UINT32),
  BluetoothProperties("UUIDs", DBUS_TYPE_ARRAY),
  BluetoothProperties("Paired", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("Connected", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("Trusted", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("Blocked", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("Alias", DBUS_TYPE_STRING),
  BluetoothProperties("Nodes", DBUS_TYPE_ARRAY),
  BluetoothProperties("Adapter", DBUS_TYPE_OBJECT_PATH),
  BluetoothProperties("LegacyPairing", DBUS_TYPE_BOOLEAN),
  BluetoothProperties("RSSI", DBUS_TYPE_INT16),
  BluetoothProperties("TX", DBUS_TYPE_UINT32)
};

enum ADAPTER_PROPERTY {
  BT_ADAPTER_ADDRESS,
  BT_ADAPTER_NAME,
  BT_ADAPTER_CLASS,
  BT_ADAPTER_POWERED,
  BT_ADAPTER_DISCOVERABLE,
  BT_ADAPTER_DISCOVERABLE_TIMEOUT,
  BT_ADAPTER_PAIRABLE,
  BT_ADAPTER_PAIRABLE_TIMEOUT,
  BT_ADAPTER_DISCOVERING,
  BT_ADAPTER_DEVICES,
  BT_ADAPTER_UUIDS
};

#endif
