#ifndef _BLUETOOTH_PROPERTIES_H_
#define _BLUETOOTH_PROPERTIES_H_

class BluetoothProperties {
  public:
    BluetoothProperties(const char* aPropertyName, int aType) : mPropertyName(aPropertyName), 
                                                                mType(aType)
    {
    
    }

    const char* mPropertyName;
    int mType;
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

#endif
