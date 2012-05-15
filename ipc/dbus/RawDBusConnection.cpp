/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RawDBusConnection.h"
#include <dbus/dbus.h>

#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"

#if defined(MOZ_WIDGET_GONK)
  #include <android/log.h>
    #define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
      #define LOG(args...)  printf(args);
#endif

using namespace mozilla::ipc;

RawDBusConnection::RawDBusConnection() {
}

RawDBusConnection::~RawDBusConnection() {
}

bool RawDBusConnection::Create() {
  DBusError err;
  dbus_error_init(&err);
  mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return false;
  }
  dbus_connection_set_exit_on_disconnect(mConnection, FALSE);
  return true;
}

