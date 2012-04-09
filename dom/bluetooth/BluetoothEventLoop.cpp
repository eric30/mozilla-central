#include <stdint.h>
#include "dbus/dbus.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Bluetooth", args)
#else
#define LOG(args...)  printf(args);
#endif

DBusHandlerResult
agent_event_filter(DBusConnection *conn, DBusMessage *msg, void *data){
  bool success = true;

  if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
    LOG("%s: not interested (not a method call).", __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  LOG("%s: Received method %s:%s", __FUNCTION__,
      dbus_message_get_interface(msg), dbus_message_get_member(msg));

  if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "RequestConfirmation")) {
    char *object_path;
    uint32_t passkey;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_UINT32, &passkey,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestConfirmation() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    LOG("ObjectPath:%s, passkey:%d", object_path, passkey);

    // TODO(Eric)
    // Now we confirm immediately, but an event is needed for notifying JS.
    DBusMessage* reply;

    if (true) {
      reply = dbus_message_new_method_return(msg);
    } else {
      reply = dbus_message_new_error(msg,
          "org.bluez.Error.Rejected", "User rejected confirmation");
    }
    if (!reply) {
      LOG("%s: Cannot create message reply to RequestPasskeyConfirmation or"
          "RequestPairingConsent to D-Bus\n", __FUNCTION__);
      return DBUS_HANDLER_RESULT_HANDLED;
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "Release")) {
    // reply
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
      LOG("%s: Cannot create message reply\n", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "Cancel")) {
    // TODO(Eric)
    // Need to modify JS that the request has been cancelled.
    //env->CallVoidMethod(nat->me, method_onAgentCancel);

    // reply
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
      LOG("%s: Cannot create message reply\n", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "Authorize")) {
    char *object_path;
    const char *uuid;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_STRING, &uuid,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for Authorize() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    LOG("... object_path = %s", object_path);
    LOG("... uuid = %s", uuid);

    // TODO(Eric)
    // Need to notify JS that Authorize event has been received.
    /*
    dbus_message_ref(msg);  // increment refcount because we pass to java
    env->CallBooleanMethod(nat->me, method_onAgentAuthorize,
        env->NewStringUTF(object_path), env->NewStringUTF(uuid),
        int(msg));
     */
  }else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "RequestPinCode")) {
    char *object_path;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestPinCode() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // TODO(Eric)
    // Need to send a request PIN code event to JS
    /*
    dbus_message_ref(msg);  // increment refcount because we pass to java
    env->CallVoidMethod(nat->me, method_onRequestPinCode,
        env->NewStringUTF(object_path),
        int(msg));*/
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "RequestPasskey")) {
    char *object_path;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestPasskey() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // TODO(Eric)
    // Need to send a request Passkey event to JS
    /*
    dbus_message_ref(msg);  // increment refcount because we pass to java
    env->CallVoidMethod(nat->me, method_onRequestPasskey,
        env->NewStringUTF(object_path),
        int(msg));
    */
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "RequestOobData")) {
    char *object_path;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestOobData() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // TODO(Eric)
    // Need to send a RequestOobData event to JS
    /*
    dbus_message_ref(msg);  // increment refcount because we pass to java
    env->CallVoidMethod(nat->me, method_onRequestOobData,
        env->NewStringUTF(object_path),
        int(msg));
    */
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "DisplayPasskey")) {
    char *object_path;
    uint32_t passkey;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_UINT32, &passkey,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestPasskey() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // TODO(Eric)
    // Need to send a RequestPasskey event to JS
/*
    dbus_message_ref(msg);  // increment refcount because we pass to java
    env->CallVoidMethod(nat->me, method_onDisplayPasskey,
        env->NewStringUTF(object_path),
        passkey,
        int(msg));
    */
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "RequestPairingConsent")) {
    char *object_path;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestPairingConsent() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
/*
    dbus_message_ref(msg);  // increment refcount because we pass to java
    env->CallVoidMethod(nat->me, method_onRequestPairingConsent,
        env->NewStringUTF(object_path),
        int(msg));
    goto success;
    */
  } else if (dbus_message_is_method_call(msg,
        "org.bluez.Agent", "OutOfBandAvailable")) {
    char *object_path;
    if (!dbus_message_get_args(msg, NULL,
          DBUS_TYPE_OBJECT_PATH, &object_path,
          DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for OutOfBandData available() method", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    LOG("... object_path = %s", object_path);

    // TODO(Eric)
    //    Needs to get the out of band settings.
    /*
    bool available =
      env->CallBooleanMethod(nat->me, method_onAgentOutOfBandDataAvailable,
          env->NewStringUTF(object_path));
    */
    bool available = false;
    DBusMessage *reply;

    // reply
    if (available) {
      reply = dbus_message_new_method_return(msg);
    } else {
      reply = dbus_message_new_error(msg,
          "org.bluez.Error.DoesNotExist", "OutofBand data not available");
    }

    if (!reply) {
      LOG("%s: Cannot create message reply\n", __FUNCTION__);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
  } 

  return DBUS_HANDLER_RESULT_HANDLED;
}
