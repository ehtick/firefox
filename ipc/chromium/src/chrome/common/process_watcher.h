/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROCESS_WATCHER_H_
#define CHROME_COMMON_PROCESS_WATCHER_H_

#include "base/basictypes.h"
#include "base/process_util.h"
#ifdef XP_UNIX
#  include "mozilla/UniquePtrExtensions.h"
#endif

class ProcessWatcher {
 public:
#ifdef NS_FREE_PERMANENT_DATA
  static constexpr bool kDefaultForce = false;
#else
  static constexpr bool kDefaultForce = true;
#endif

  // This method ensures that the specified process eventually terminates, and
  // then it closes the given process handle.
  //
  // It assumes that the process has already been signalled to exit, and it
  // begins by waiting a small amount of time for it to exit.  If the process
  // does not appear to have exited, then this function starts to become
  // aggressive about ensuring that the process terminates.
  //
  // This method does not block the calling thread.
  //
  // NOTE: The process handle must have been opened with the PROCESS_TERMINATE
  // and SYNCHRONIZE permissions.
  //
  static void EnsureProcessTerminated(base::ProcessHandle process_handle,
                                      bool force = kDefaultForce);

#ifdef XP_UNIX
  static mozilla::UniqueFileHandle GetSignalPipe();
#endif

 private:
  // Do not instantiate this class.
  ProcessWatcher();

  DISALLOW_COPY_AND_ASSIGN(ProcessWatcher);
};

#endif  // CHROME_COMMON_PROCESS_WATCHER_H_
