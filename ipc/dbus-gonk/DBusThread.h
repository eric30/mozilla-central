/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_dbus_gonk_dbusthread_h__
#define mozilla_ipc_dbus_gonk_dbusthread_h__

#include "RawDBusConnection.h"
#include "pthread.h"

struct DBusMessage;
struct DBusWatch;
struct pollfd;

namespace mozilla {
namespace ipc {

class DBusThread : protected RawDBusConnection {
public:
	DBusThread();
	~DBusThread();
	bool setUpEventLoop();
	bool startEventLoop();
	void stopEventLoop();
	bool isEventLoopRunning();
	void EventFilter(DBusMessage* msg);
  static void* eventLoop(void* ptr);
	/* protects the thread */
	pthread_mutex_t thread_mutex;
	pthread_t thread;
	/* our comms socket */
	/* mem for the list of sockets to listen to */
	struct pollfd *pollData;
	int pollMemberCount;
	int pollDataSize;
	/* mem for matching set of dbus watch ptrs */
	DBusWatch **watchData;
	/* pair of sockets for event loop control, Reader and Writer */
	int controlFdR;
	int controlFdW;
	bool running;
};

}
}
#endif
