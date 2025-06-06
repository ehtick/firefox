/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=8 et tw=80 :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ChannelEventQueue_h
#define mozilla_net_ChannelEventQueue_h

#include "nsTArray.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Mutex.h"
#include "mozilla/RecursiveMutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/FlowMarkers.h"

class nsISupports;

namespace mozilla {
namespace net {

class ChannelEvent {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(ChannelEvent)
  MOZ_COUNTED_DTOR_VIRTUAL(ChannelEvent) virtual void Run() = 0;
  virtual already_AddRefed<nsIEventTarget> GetEventTarget() = 0;
};

// Note that MainThreadChannelEvent should not be used in child process since
// GetEventTarget() directly returns an unlabeled event target.
class MainThreadChannelEvent : public ChannelEvent {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(MainThreadChannelEvent)
  MOZ_COUNTED_DTOR_OVERRIDE(MainThreadChannelEvent)

  already_AddRefed<nsIEventTarget> GetEventTarget() override {
    MOZ_ASSERT(XRE_IsParentProcess());

    return do_AddRef(GetMainThreadSerialEventTarget());
  }
};

class ChannelFunctionEvent : public ChannelEvent {
 public:
  ChannelFunctionEvent(
      std::function<already_AddRefed<nsIEventTarget>()>&& aGetEventTarget,
      std::function<void()>&& aCallback)
      : mGetEventTarget(std::move(aGetEventTarget)),
        mCallback(std::move(aCallback)) {}

  void Run() override { mCallback(); }
  already_AddRefed<nsIEventTarget> GetEventTarget() override {
    return mGetEventTarget();
  }

 private:
  const std::function<already_AddRefed<nsIEventTarget>()> mGetEventTarget;
  const std::function<void()> mCallback;
};

// UnsafePtr is a work-around our static analyzer that requires all
// ref-counted objects to be captured in lambda via a RefPtr
// The ChannelEventQueue makes it safe to capture "this" by pointer only.
// This is required as work-around to prevent cycles until bug 1596295
// is resolved.
template <typename T>
class UnsafePtr {
 public:
  explicit UnsafePtr(T* aPtr) : mPtr(aPtr) {}

  T& operator*() const { return *mPtr; }
  T* operator->() const {
    MOZ_ASSERT(mPtr, "dereferencing a null pointer");
    return mPtr;
  }
  operator T*() const& { return mPtr; }
  explicit operator bool() const { return mPtr != nullptr; }

 private:
  T* const mPtr;
};

class NeckoTargetChannelFunctionEvent : public ChannelFunctionEvent {
 public:
  template <typename T>
  NeckoTargetChannelFunctionEvent(T* aChild, std::function<void()>&& aCallback)
      : ChannelFunctionEvent(
            [child = UnsafePtr<T>(aChild)]() {
              MOZ_ASSERT(child);
              return child->GetNeckoTarget();
            },
            std::move(aCallback)) {}
};

// Workaround for Necko re-entrancy dangers. We buffer IPDL messages in a
// queue if still dispatching previous one(s) to listeners/observers.
// Otherwise synchronous XMLHttpRequests and/or other code that spins the
// event loop (ex: IPDL rpc) could cause listener->OnDataAvailable (for
// instance) to be dispatched and called before mListener->OnStartRequest has
// completed.
// The ChannelEventQueue implementation ensures strict ordering of
// event execution across target threads.

class ChannelEventQueue final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChannelEventQueue)

 public:
  explicit ChannelEventQueue(nsISupports* owner)
      : mSuspendCount(0),
        mSuspended(false),
        mForcedCount(0),
        mFlushing(false),
        mHasCheckedForAsyncXMLHttpRequest(false),
        mForAsyncXMLHttpRequest(false),
        mOwner(owner),
        mMutex("ChannelEventQueue::mMutex"),
        mRunningMutex("ChannelEventQueue::mRunningMutex") {}

  // Puts IPDL-generated channel event into queue, to be run later
  // automatically when EndForcedQueueing and/or Resume is called.
  //
  // @param aCallback - the ChannelEvent
  // @param aAssertionWhenNotQueued - this optional param will be used in an
  //   assertion when the event is executed directly.
  inline void RunOrEnqueue(ChannelEvent* aCallback,
                           bool aAssertionWhenNotQueued = false);

  // Append ChannelEvent in front of the event queue.
  inline void PrependEvent(UniquePtr<ChannelEvent>&& aEvent);
  inline void PrependEventInternal(UniquePtr<ChannelEvent>&& aEvent)
      MOZ_REQUIRES(mMutex);
  inline void PrependEvents(nsTArray<UniquePtr<ChannelEvent>>& aEvents);

  // After StartForcedQueueing is called, RunOrEnqueue() will start enqueuing
  // events that will be run/flushed when EndForcedQueueing is called.
  // - Note: queueing may still be required after EndForcedQueueing() (if the
  //   queue is suspended, etc):  always call RunOrEnqueue() to avoid race
  //   conditions.
  inline void StartForcedQueueing();
  inline void EndForcedQueueing();

  // Suspend/resume event queue.  RunOrEnqueue() will start enqueuing
  // events and they will be run/flushed when resume is called.  These should be
  // called when the channel owning the event queue is suspended/resumed.
  void Suspend();
  // Resume flushes the queue asynchronously, i.e. items in queue will be
  // dispatched in a new event on the current thread.
  void Resume();

  void NotifyReleasingOwner() {
    MutexAutoLock lock(mMutex);
    mOwner = nullptr;
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool IsEmpty() {
    MutexAutoLock lock(mMutex);
    return mEventQueue.IsEmpty();
  }
#endif

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~ChannelEventQueue() = default;

  void SuspendInternal() MOZ_REQUIRES(mMutex);
  void ResumeInternal() MOZ_REQUIRES(mMutex);

  bool MaybeSuspendIfEventsAreSuppressed() MOZ_REQUIRES(mMutex);

  inline void MaybeFlushQueue() MOZ_REQUIRES(mMutex);
  void FlushQueue() MOZ_REQUIRES(mMutex);
  inline void CompleteResume();

  ChannelEvent* TakeEvent();

  nsTArray<UniquePtr<ChannelEvent>> mEventQueue MOZ_GUARDED_BY(mMutex);

  uint32_t mSuspendCount MOZ_GUARDED_BY(mMutex);
  bool mSuspended MOZ_GUARDED_BY(mMutex);
  uint32_t mForcedCount  // Support ForcedQueueing on multiple thread.
      MOZ_GUARDED_BY(mMutex);
  bool mFlushing MOZ_GUARDED_BY(mMutex);

  // Whether the queue is associated with an ssync XHR.
  // This is lazily instantiated the first time it is needed.
  // These are MainThread-only.
  bool mHasCheckedForAsyncXMLHttpRequest;
  bool mForAsyncXMLHttpRequest;

  // Keep ptr to avoid refcount cycle: only grab ref during flushing.
  nsISupports* mOwner MOZ_GUARDED_BY(mMutex);

  // For atomic mEventQueue operation and state update
  Mutex mMutex;

  // To guarantee event execution order among threads
  RecursiveMutex mRunningMutex MOZ_ACQUIRED_BEFORE(mMutex);

  friend class AutoEventEnqueuer;
};

inline void ChannelEventQueue::RunOrEnqueue(ChannelEvent* aCallback,
                                            bool aAssertionWhenNotQueued) {
  MOZ_ASSERT(aCallback);
  // Events execution could be a destruction of the channel (and our own
  // destructor) unless we make sure its refcount doesn't drop to 0 while this
  // method is running.
  nsCOMPtr<nsISupports> kungFuDeathGrip;

  // To avoid leaks.
  UniquePtr<ChannelEvent> event(aCallback);

  // To guarantee that the running event and all the events generated within
  // it will be finished before events on other threads.
  RecursiveMutexAutoLock lock(mRunningMutex);
  {
    MutexAutoLock lock(mMutex);
    kungFuDeathGrip = mOwner;  // must be under the lock

    bool enqueue = !!mForcedCount || mSuspended || mFlushing ||
                   !mEventQueue.IsEmpty() ||
                   MaybeSuspendIfEventsAreSuppressed();
    // To ensure strict ordering of events across multiple threads we buffer the
    // events for the below cases:
    // a. event queuing is forced by AutoEventEnqueuer
    // b. event queue is suspended
    // c. an event is currently flushed/executed from the queue
    // d. queue is non-empty (pending events on remote thread targets)
    if (enqueue) {
      PROFILER_MARKER("ChannelEventQueue::Enqueue", NETWORK, {}, FlowMarker,
                      Flow::FromPointer(event.get()));
      mEventQueue.AppendElement(std::move(event));
      return;
    }

    nsCOMPtr<nsIEventTarget> target = event->GetEventTarget();
    MOZ_ASSERT(target);

    bool isCurrentThread = false;
    DebugOnly<nsresult> rv = target->IsOnCurrentThread(&isCurrentThread);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (!isCurrentThread) {
      // Leverage Suspend/Resume mechanism to trigger flush procedure without
      // creating a new one.
      // The execution of further events in the queue is blocked until the
      // target thread completes the execution of this event.
      // A callback is dispatched to the target thread to flush events from the
      // queue. This is done
      // by ResumeInternal which dispatches a runnable
      // (CompleteResumeRunnable) to the target thread. The target thread will
      // call CompleteResume to flush the queue. All the events are run
      // synchronously in their respective target threads.
      SuspendInternal();

      PROFILER_MARKER("ChannelEventQueue::Enqueue", NETWORK, {}, FlowMarker,
                      Flow::FromPointer(event.get()));

      mEventQueue.AppendElement(std::move(event));
      ResumeInternal();
      return;
    }
  }

  MOZ_RELEASE_ASSERT(!aAssertionWhenNotQueued);
  AUTO_PROFILER_TERMINATING_FLOW_MARKER("ChannelEvent", OTHER,
                                        Flow::FromPointer(event.get()));
  // execute the event synchronously if we are not queuing it and
  // the target thread is the current thread
  event->Run();
}

inline void ChannelEventQueue::StartForcedQueueing() {
  MutexAutoLock lock(mMutex);
  ++mForcedCount;
}

inline void ChannelEventQueue::EndForcedQueueing() {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mForcedCount > 0);
  if (!--mForcedCount) {
    MaybeFlushQueue();
  }
}

inline void ChannelEventQueue::PrependEvent(UniquePtr<ChannelEvent>&& aEvent) {
  MutexAutoLock lock(mMutex);
  PrependEventInternal(std::move(aEvent));
}

inline void ChannelEventQueue::PrependEventInternal(
    UniquePtr<ChannelEvent>&& aEvent) {
  mMutex.AssertCurrentThreadOwns();

  // Prepending event while no queue flush foreseen might cause the following
  // channel events not run. This assertion here guarantee there must be a
  // queue flush, either triggered by Resume or EndForcedQueueing, to execute
  // the added event.
  MOZ_ASSERT(mSuspended || !!mForcedCount);

  mEventQueue.InsertElementAt(0, std::move(aEvent));
}

inline void ChannelEventQueue::PrependEvents(
    nsTArray<UniquePtr<ChannelEvent>>& aEvents) {
  MutexAutoLock lock(mMutex);

  // Prepending event while no queue flush foreseen might cause the following
  // channel events not run. This assertion here guarantee there must be a
  // queue flush, either triggered by Resume or EndForcedQueueing, to execute
  // the added events.
  MOZ_ASSERT(mSuspended || !!mForcedCount);

  mEventQueue.InsertElementsAt(0, aEvents.Length());

  for (uint32_t i = 0; i < aEvents.Length(); i++) {
    mEventQueue[i] = std::move(aEvents[i]);
  }
}

inline void ChannelEventQueue::CompleteResume() {
  MutexAutoLock lock(mMutex);

  // channel may have been suspended again since Resume fired event to call
  // this.
  if (!mSuspendCount) {
    // we need to remain logically suspended (for purposes of queuing incoming
    // messages) until this point, else new incoming messages could run before
    // queued ones.
    mSuspended = false;
    MaybeFlushQueue();
  }
}

inline void ChannelEventQueue::MaybeFlushQueue() {
  mMutex.AssertCurrentThreadOwns();
  // Don't flush if forced queuing on, we're already being flushed, or
  // suspended, or there's nothing to flush
  bool flushQueue = !mForcedCount && !mFlushing && !mSuspended &&
                    !mEventQueue.IsEmpty() &&
                    !MaybeSuspendIfEventsAreSuppressed();

  // Only one thread is allowed to run FlushQueue at a time.
  if (flushQueue) {
    mFlushing = true;
    FlushQueue();
  }
}

// Ensures that RunOrEnqueue() will be collecting events during its lifetime
// (letting caller know incoming IPDL msgs should be queued). Flushes the
// queue when it goes out of scope.
class MOZ_STACK_CLASS AutoEventEnqueuer {
 public:
  explicit AutoEventEnqueuer(ChannelEventQueue* queue) : mEventQueue(queue) {
    {
      // Probably not actually needed, since NotifyReleasingOwner should
      // only happen after this, but safer to take it in case things change
      MutexAutoLock lock(queue->mMutex);
      mOwner = queue->mOwner;
    }
    mEventQueue->StartForcedQueueing();
  }
  ~AutoEventEnqueuer() { mEventQueue->EndForcedQueueing(); }

 private:
  RefPtr<ChannelEventQueue> mEventQueue;
  // Ensure channel object lives longer than ChannelEventQueue.
  nsCOMPtr<nsISupports> mOwner;
};

}  // namespace net
}  // namespace mozilla

#endif
