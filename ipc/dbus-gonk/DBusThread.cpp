/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBusThread.h"
#include <dbus/dbus.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>

// size of the dbus event loops pollfd structure, hopefully never to be grown
#define DEFAULT_INITIAL_POLLFD_COUNT 8

#define EVENT_LOOP_EXIT 1
#define EVENT_LOOP_ADD  2
#define EVENT_LOOP_REMOVE 3

static unsigned int unix_events_to_dbus_flags(short events) {
	return (events & DBUS_WATCH_READABLE ? POLLIN : 0) |
		(events & DBUS_WATCH_WRITABLE ? POLLOUT : 0) |
		(events & DBUS_WATCH_ERROR ? POLLERR : 0) |
		(events & DBUS_WATCH_HANGUP ? POLLHUP : 0);
}

static short dbus_flags_to_unix_events(unsigned int flags) {
	return (flags & POLLIN ? DBUS_WATCH_READABLE : 0) |
		(flags & POLLOUT ? DBUS_WATCH_WRITABLE : 0) |
		(flags & POLLERR ? DBUS_WATCH_ERROR : 0) |
		(flags & POLLHUP ? DBUS_WATCH_HANGUP : 0);
}

using namespace mozilla::ipc;

void DBusThread::EventFilter(DBusMessage* msg) {
	printf("I SHOULD NOT BE BEING CALLED\n");
}

DBusThread::DBusThread()
{
	createDBusConnection();
	pthread_mutex_init(&(thread_mutex), NULL);
	pollData = NULL;
}

DBusThread::~DBusThread()
{
}

static dbus_bool_t addWatch(DBusWatch *watch, void *data) {
	printf("add Watch!\n");
	DBusThread *dbt = (DBusThread *)data;

	if (dbus_watch_get_enabled(watch)) {
			printf("enabled Watch!\n");
		// note that we can't just send the watch and inspect it later
		// because we may get a removeWatch call before this data is reacted
		// to by our eventloop and remove this watch..  reading the add first
		// and then inspecting the recently deceased watch would be bad.
		char control = EVENT_LOOP_ADD;
		write(dbt->controlFdW, &control, sizeof(char));

		int fd = dbus_watch_get_fd(watch);
		write(dbt->controlFdW, &fd, sizeof(int));

		unsigned int flags = dbus_watch_get_flags(watch);
		write(dbt->controlFdW, &flags, sizeof(unsigned int));

		write(dbt->controlFdW, &watch, sizeof(DBusWatch*));
	}
	return true;
}

static void removeWatch(DBusWatch *watch, void *data) {
	printf("remove Watch!\n");
	DBusThread *dbt = (DBusThread *)data;

	char control = EVENT_LOOP_REMOVE;
	write(dbt->controlFdW, &control, sizeof(char));

	int fd = dbus_watch_get_fd(watch);
	write(dbt->controlFdW, &fd, sizeof(int));

	unsigned int flags = dbus_watch_get_flags(watch);
	write(dbt->controlFdW, &flags, sizeof(unsigned int));
}

static void toggleWatch(DBusWatch *watch, void *data) {
	printf("toggle Watch!\n");
	if (dbus_watch_get_enabled(watch)) {
		addWatch(watch, data);
	} else {
		removeWatch(watch, data);
	}
}

static void handleWatchAdd(DBusThread* dbt) {
	DBusWatch *watch;
	int newFD;
	unsigned int flags;
	printf("Handling watch addition!\n");
	read(dbt->controlFdR, &newFD, sizeof(int));
	read(dbt->controlFdR, &flags, sizeof(unsigned int));
	read(dbt->controlFdR, &watch, sizeof(DBusWatch *));
	short events = dbus_flags_to_unix_events(flags);

	for (int y = 0; y<dbt->pollMemberCount; y++) {
		if ((dbt->pollData[y].fd == newFD) &&
				(dbt->pollData[y].events == events)) {
			printf("DBusWatch duplicate add\n");
			return;
		}
	}
	if (dbt->pollMemberCount == dbt->pollDataSize) {
		printf("Bluetooth EventLoop poll struct growing\n");
		struct pollfd *temp = (struct pollfd *)malloc(
			sizeof(struct pollfd) * (dbt->pollMemberCount+1));
		if (!temp) {
			return;
		}
		memcpy(temp, dbt->pollData, sizeof(struct pollfd) *
					 dbt->pollMemberCount);
		free(dbt->pollData);
		dbt->pollData = temp;
		DBusWatch **temp2 = (DBusWatch **)malloc(sizeof(DBusWatch *) *
																						 (dbt->pollMemberCount+1));
		if (!temp2) {
			return;
		}
		memcpy(temp2, dbt->watchData, sizeof(DBusWatch *) *
					 dbt->pollMemberCount);
		free(dbt->watchData);
		dbt->watchData = temp2;
		dbt->pollDataSize++;
	}
	dbt->pollData[dbt->pollMemberCount].fd = newFD;
	dbt->pollData[dbt->pollMemberCount].revents = 0;
	dbt->pollData[dbt->pollMemberCount].events = events;
	dbt->watchData[dbt->pollMemberCount] = watch;
	dbt->pollMemberCount++;
}

// Called by dbus during WaitForAndDispatchEventNative()
static DBusHandlerResult event_filter(DBusConnection *conn, DBusMessage *msg,
																					 void *data) {
//     native_data_t *nat;
//     JNIEnv *env;
	DBusError err;
	DBusHandlerResult ret;

	dbus_error_init(&err);

//     nat = (native_data_t *)data;
//     dbt->vm->GetEnv((void**)&env, dbt->envVer);
	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL) {
		printf("%s: not interested (not a signal).\n", __FUNCTION__);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	printf("%s: Received signal %s:%s from %s\n", __FUNCTION__,
				 dbus_message_get_interface(msg), dbus_message_get_member(msg),
				 dbus_message_get_path(msg));

//     env->PushLocalFrame(EVENT_LOOP_REFS);
//     if (dbus_message_is_signal(msg,
//                                "org.bluez.Adapter",
//                                "DeviceFound")) {
//         char *c_address;
//         DBusMessageIter iter;
//         jobjectArray str_array = NULL;
//         if (dbus_message_iter_init(msg, &iter)) {
//             dbus_message_iter_get_basic(&iter, &c_address);
//             if (dbus_message_iter_next(&iter))
//                 str_array =
//                     parse_remote_device_properties(env, &iter);
//         }
//         if (str_array != NULL) {
//             env->CallVoidMethod(dbt->me,
//                                 method_onDeviceFound,
//                                 env->NewStringUTF(c_address),
//                                 str_array);
//         } else
//             LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
//         goto success;
//     } else if (dbus_message_is_signal(msg,
//                                      "org.bluez.Adapter",
//                                      "DeviceDisappeared")) {
//         char *c_address;
//         if (dbus_message_get_args(msg, &err,
//                                   DBUS_TYPE_STRING, &c_address,
//                                   DBUS_TYPE_INVALID)) {
//             LOGV("... address = %s", c_address);
//             env->CallVoidMethod(dbt->me, method_onDeviceDisappeared,
//                                 env->NewStringUTF(c_address));
//         } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
//         goto success;
//     } else if (dbus_message_is_signal(msg,
//                                      "org.bluez.Adapter",
//                                      "DeviceCreated")) {
//         char *c_object_path;
//         if (dbus_message_get_args(msg, &err,
//                                   DBUS_TYPE_OBJECT_PATH, &c_object_path,
//                                   DBUS_TYPE_INVALID)) {
//             LOGV("... address = %s", c_object_path);
//             env->CallVoidMethod(dbt->me,
//                                 method_onDeviceCreated,
//                                 env->NewStringUTF(c_object_path));
//         } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
//         goto success;
//     } else if (dbus_message_is_signal(msg,
//                                      "org.bluez.Adapter",
//                                      "DeviceRemoved")) {
//         char *c_object_path;
//         if (dbus_message_get_args(msg, &err,
//                                  DBUS_TYPE_OBJECT_PATH, &c_object_path,
//                                  DBUS_TYPE_INVALID)) {
//            LOGV("... Object Path = %s", c_object_path);
//            env->CallVoidMethod(dbt->me,
//                                method_onDeviceRemoved,
//                                env->NewStringUTF(c_object_path));
//         } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
//         goto success;
//     } else if (dbus_message_is_signal(msg,
//                                       "org.bluez.Adapter",
//                                       "PropertyChanged")) {
//         jobjectArray str_array = parse_adapter_property_change(env, msg);
//         if (str_array != NULL) {
//             /* Check if bluetoothd has (re)started, if so update the path. */
//             jstring property =(jstring) env->GetObjectArrayElement(str_array, 0);
//             const char *c_property = env->GetStringUTFChars(property, NULL);
//             if (!strncmp(c_property, "Powered", strlen("Powered"))) {
//                 jstring value =
//                     (jstring) env->GetObjectArrayElement(str_array, 1);
//                 const char *c_value = env->GetStringUTFChars(value, NULL);
//                 if (!strncmp(c_value, "true", strlen("true")))
//                     dbt->adapter = get_adapter_path(dbt->conn);
//                 env->ReleaseStringUTFChars(value, c_value);
//             }
//             env->ReleaseStringUTFChars(property, c_property);

//             env->CallVoidMethod(dbt->me,
//                               method_onPropertyChanged,
//                               str_array);
//         } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
//         goto success;
//     } else if (dbus_message_is_signal(msg,
//                                       "org.bluez.Device",
//                                       "PropertyChanged")) {
//         jobjectArray str_array = parse_remote_device_property_change(env, msg);
//         if (str_array != NULL) {
//             const char *remote_device_path = dbus_message_get_path(msg);
//             env->CallVoidMethod(dbt->me,
//                             method_onDevicePropertyChanged,
//                             env->NewStringUTF(remote_device_path),
//                             str_array);
//         } else LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, msg);
//         goto success;
//     } else if (dbus_message_is_signal(msg,
//                                       "org.bluez.Device",
//                                       "DisconnectRequested")) {
//         const char *remote_device_path = dbus_message_get_path(msg);
//         env->CallVoidMethod(dbt->me,
//                             method_onDeviceDisconnectRequested,
//                             env->NewStringUTF(remote_device_path));
//         goto success;
//     }

//     ret = a2dp_event_filter(msg, env);
//     env->PopLocalFrame(NULL);
//     return ret;

// success:
//     env->PopLocalFrame(NULL);
	return DBUS_HANDLER_RESULT_HANDLED;
}


bool
DBusThread::setUpEventLoop()
{
	if(!mConnection) {
		return false;
	}
		
	dbus_threads_init_default();
	DBusError err;
	dbus_error_init(&err);

	// Add a filter for all incoming messages_base
	if (!dbus_connection_add_filter(mConnection, event_filter, this, NULL)){
		return false;
	}

	// // Set which messages will be processed by this dbus connection
	// dbus_bus_add_match(mConnection,
	// 									 "type='signal',interface='org.freedesktop.DBus'",
	// 									 &err);
	// if (dbus_error_is_set(&err)) {
	// 	//LOG_AND_FREE_DBUS_ERROR(&err);
	// 	return false;
	// }
	// dbus_bus_add_match(mConnection,
	// 									 "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".Adapter'",
	// 									 &err);
	// if (dbus_error_is_set(&err)) {
	// 	return false;
	// }
	// dbus_bus_add_match(mConnection,
	// 									 "type='signal',interface='"BLUEZ_DBUS_BASE_IFC".Device'",
	// 									 &err);
	// if (dbus_error_is_set(&err)) {
	// 	return false;
	// }
	// dbus_bus_add_match(mConnection,
	// 									 "type='signal',interface='org.bluez.AudioSink'",
	// 									 &err);
	// if (dbus_error_is_set(&err)) {
	// 	return false;
	// }
	
	// const char *agent_path = "/android/bluetooth/agent";
	// const char *capabilities = "DisplayYesNo";
	// if (register_agent(nat, agent_path, capabilities) < 0) {
	// 	dbus_connection_unregister_object_path (dbt->conn, agent_path);
	// 	return JNI_FALSE;
	// }
	return true;
}

void* DBusThread::eventLoop(void *ptr) {

	char name[] = "BT EventLoop";
	DBusThread* dbt = (DBusThread*)ptr;
	bool result;
	dbus_connection_set_watch_functions(dbt->mConnection, addWatch,
																			removeWatch, toggleWatch, ptr, NULL);

	dbt->running = true;
	
	while (1) {
		for (int i = 0; i < dbt->pollMemberCount; i++) {
			if (!dbt->pollData[i].revents) {
				continue;
			}

			if (dbt->pollData[i].fd == dbt->controlFdR) {
				printf("Got a watch request!\n");
				char data;
				while (recv(dbt->controlFdR, &data, sizeof(char), MSG_DONTWAIT)
							 != -1) {
					switch (data) {
					case EVENT_LOOP_EXIT:
					{
						dbus_connection_set_watch_functions(dbt->mConnection,
																								NULL, NULL, NULL, NULL, NULL);
						// tearDownEventLoop(nat);
						int fd = dbt->controlFdR;
						dbt->controlFdR = 0;
						close(fd);
						return NULL;
					}
					case EVENT_LOOP_ADD:
					{
						handleWatchAdd(dbt);
						break;
					}
					case EVENT_LOOP_REMOVE:
					{
						// handleWatchRemove(nat);
						break;
					}
					}
				}
			} else {
				printf("Got an watch handle!\n");
				short events = dbt->pollData[i].revents;
				unsigned int flags = unix_events_to_dbus_flags(events);
				dbus_watch_handle(dbt->watchData[i], flags);
				dbt->pollData[i].revents = 0;
				// can only do one - it may have caused a 'remove'
				break;
			}
		}
		while (dbus_connection_dispatch(dbt->mConnection) ==
					 DBUS_DISPATCH_DATA_REMAINS) {
			printf("Got an event!\n");

		}

		poll(dbt->pollData, dbt->pollMemberCount, -1);
	}
}

bool DBusThread::startEventLoop() {
	pthread_mutex_lock(&(thread_mutex));
	bool result = false;
	running = false;
	printf("Hey, what's controlFdR right now? %d\n", controlFdR);
	if (pollData) {
		printf("trying to start EventLoop a second time!\n");
		pthread_mutex_unlock( &(thread_mutex) );
		return false;
	}
	pollData = (struct pollfd *)malloc(sizeof(struct pollfd) *
																		 DEFAULT_INITIAL_POLLFD_COUNT);
	if (!pollData) {
		printf("out of memory error starting EventLoop!\n");
		goto done;
	}

	watchData = (DBusWatch **)malloc(sizeof(DBusWatch *) *
																	 DEFAULT_INITIAL_POLLFD_COUNT);
	if (!watchData) {
		printf("out of memory error starting EventLoop!\n");
		goto done;
	}

	memset(pollData, 0, sizeof(struct pollfd) *
				 DEFAULT_INITIAL_POLLFD_COUNT);
	memset(watchData, 0, sizeof(DBusWatch *) *
				 DEFAULT_INITIAL_POLLFD_COUNT);
	pollDataSize = DEFAULT_INITIAL_POLLFD_COUNT;
	pollMemberCount = 1;
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, &(controlFdR))) {
		printf("Error getting BT control socket\n");
		goto done;
	}
	pollData[0].fd = controlFdR;
	pollData[0].events = POLLIN;
	if (setUpEventLoop() != true) {
		printf("failure setting up Event Loop!\n");
		goto done;
	}
	printf("Control file descriptor: %d\n", controlFdR);
	pthread_create(&(thread), NULL, DBusThread::eventLoop, this);
	result = true;
done:
	if (false == result) {
		if (controlFdW) {
			close(controlFdW);
			controlFdW = 0;
		}
		if (controlFdR) {
			close(controlFdR);
			controlFdR = 0;
		}
		if (pollData) free(pollData);
		pollData = NULL;
		if (watchData) free(watchData);
		watchData = NULL;
		pollDataSize = 0;
		pollMemberCount = 0;
	}

	pthread_mutex_unlock(&(thread_mutex));

	return result;
}

void DBusThread::stopEventLoop() {

	pthread_mutex_lock(&(thread_mutex));
	if (pollData) {
		char data = EVENT_LOOP_EXIT;
		ssize_t t = write(controlFdW, &data, sizeof(char));
		void *ret;
		pthread_join(thread, &ret);

		free(pollData);
		pollData = NULL;
		free(watchData);
		watchData = NULL;
		pollDataSize = 0;
		pollMemberCount = 0;

		int fd = controlFdW;
		controlFdW = 0;
		close(fd);
	}
	running = false;
	pthread_mutex_unlock(&(thread_mutex));
	printf("Event loop stopped!\n");	
}

bool DBusThread::isEventLoopRunning() {

	bool result = false;
	pthread_mutex_lock(&(thread_mutex));
	if (running) {
		result = true;
	}
	pthread_mutex_unlock(&(thread_mutex));

	return result;
}
