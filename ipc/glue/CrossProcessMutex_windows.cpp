/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>

#include "base/process_util.h"
#include "CrossProcessMutex.h"
#include "nsDebug.h"
#include "nsTraceRefcnt.h"

using namespace base;

namespace mozilla {

CrossProcessMutex::CrossProcessMutex(const char*)
{
  // We explicitly share this using DuplicateHandle, we do -not- want this to
  // be inherited by child processes by default! So no security attributes are
  // given.
  mMutex = ::CreateMutexA(NULL, FALSE, NULL);
  if (!mMutex) {
    NS_RUNTIMEABORT("This shouldn't happen - failed to create mutex!");
  }
  MOZ_COUNT_CTOR(CrossProcessMutex);
}

CrossProcessMutex::CrossProcessMutex(CrossProcessMutexHandle aHandle)
{
  DWORD flags;
  if (!::GetHandleInformation(aHandle, &flags)) {
    NS_RUNTIMEABORT("Attempt to construct a mutex from an invalid handle!");
  }
  mMutex = aHandle;
  MOZ_COUNT_CTOR(CrossProcessMutex);
}

CrossProcessMutex::~CrossProcessMutex()
{
  NS_ASSERTION(mMutex, "Improper construction of mutex or double free.");
  ::CloseHandle(mMutex);
  MOZ_COUNT_DTOR(CrossProcessMutex);
}

void
CrossProcessMutex::Lock()
{
  NS_ASSERTION(mMutex, "Improper construction of mutex.");
  ::WaitForSingleObject(mMutex, INFINITE);
}

void
CrossProcessMutex::Unlock()
{
  NS_ASSERTION(mMutex, "Improper construction of mutex.");
  ::ReleaseMutex(mMutex);
}

CrossProcessMutexHandle
CrossProcessMutex::ShareToProcess(ProcessHandle aHandle)
{
  HANDLE newHandle;
  bool succeeded = ::DuplicateHandle(GetCurrentProcessHandle(),
                                     mMutex, aHandle, &newHandle,
                                     NULL, FALSE, DUPLICATE_SAME_ACCESS);

  if (!succeeded) {
    return NULL;
  }

  return newHandle;
}

}
