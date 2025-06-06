/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TouchEvents_h__
#define mozilla_TouchEvents_h__

#include <stdint.h>

#include "mozilla/dom/Touch.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {

/******************************************************************************
 * mozilla::WidgetGestureNotifyEvent
 *
 * This event is the first event generated when the user touches
 * the screen with a finger, and it's meant to decide what kind
 * of action we'll use for that touch interaction.
 *
 * The event is dispatched to the layout and based on what is underneath
 * the initial contact point it's then decided if we should pan
 * (finger scrolling) or drag the target element.
 ******************************************************************************/

class WidgetGestureNotifyEvent : public WidgetGUIEvent {
 public:
  virtual WidgetGestureNotifyEvent* AsGestureNotifyEvent() override {
    return this;
  }

  WidgetGestureNotifyEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget,
                           const WidgetEventTime* aTime = nullptr)
      : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eGestureNotifyEventClass,
                       aTime),
        mPanDirection(ePanNone),
        mDisplayPanFeedback(false) {}

  virtual WidgetEvent* Duplicate() const override {
    // XXX Looks like this event is handled only in PostHandleEvent() of
    //     EventStateManager.  Therefore, it might be possible to handle this
    //     in PreHandleEvent() and not to dispatch as a DOM event into the DOM
    //     tree like ContentQueryEvent.  Then, this event doesn't need to
    //     support Duplicate().
    MOZ_ASSERT(mClass == eGestureNotifyEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetGestureNotifyEvent* result =
        new WidgetGestureNotifyEvent(false, mMessage, nullptr, this);
    result->AssignGestureNotifyEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  typedef int8_t PanDirectionType;
  enum PanDirection : PanDirectionType {
    ePanNone,
    ePanVertical,
    ePanHorizontal,
    ePanBoth
  };

  PanDirection mPanDirection;
  bool mDisplayPanFeedback;

  void AssignGestureNotifyEventData(const WidgetGestureNotifyEvent& aEvent,
                                    bool aCopyTargets) {
    AssignGUIEventData(aEvent, aCopyTargets);

    mPanDirection = aEvent.mPanDirection;
    mDisplayPanFeedback = aEvent.mDisplayPanFeedback;
  }
};

/******************************************************************************
 * mozilla::WidgetSimpleGestureEvent
 ******************************************************************************/

class WidgetSimpleGestureEvent : public WidgetMouseEventBase {
 public:
  virtual WidgetSimpleGestureEvent* AsSimpleGestureEvent() override {
    return this;
  }

  WidgetSimpleGestureEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget,
                           const WidgetEventTime* aTime = nullptr)
      : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                             eSimpleGestureEventClass, aTime),
        mAllowedDirections(0),
        mDirection(0),
        mClickCount(0),
        mDelta(0.0) {}

  WidgetSimpleGestureEvent(const WidgetSimpleGestureEvent& aOther)
      : WidgetMouseEventBase(aOther.IsTrusted(), aOther.mMessage,
                             aOther.mWidget, eSimpleGestureEventClass),
        mAllowedDirections(aOther.mAllowedDirections),
        mDirection(aOther.mDirection),
        mClickCount(0),
        mDelta(aOther.mDelta) {}

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eSimpleGestureEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetSimpleGestureEvent* result =
        new WidgetSimpleGestureEvent(false, mMessage, nullptr, this);
    result->AssignSimpleGestureEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // See SimpleGestureEvent.webidl for values
  uint32_t mAllowedDirections;
  // See SimpleGestureEvent.webidl for values
  uint32_t mDirection;
  // The number of taps for tap events
  uint32_t mClickCount;
  // Delta for magnify and rotate events
  double mDelta;

  // XXX Not tested by test_assign_event_data.html
  void AssignSimpleGestureEventData(const WidgetSimpleGestureEvent& aEvent,
                                    bool aCopyTargets) {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    // mAllowedDirections isn't copied
    mDirection = aEvent.mDirection;
    mDelta = aEvent.mDelta;
    mClickCount = aEvent.mClickCount;
  }
};

/******************************************************************************
 * mozilla::WidgetTouchEvent
 ******************************************************************************/

class WidgetTouchEvent final : public WidgetInputEvent {
 public:
  typedef nsTArray<RefPtr<mozilla::dom::Touch>> TouchArray;
  typedef AutoTArray<RefPtr<mozilla::dom::Touch>, 10> AutoTouchArray;
  typedef AutoTouchArray::base_type TouchArrayBase;

  WidgetTouchEvent* AsTouchEvent() override { return this; }

  MOZ_COUNTED_DEFAULT_CTOR(WidgetTouchEvent)

  enum class CloneTouches : bool { No, Yes };
  WidgetTouchEvent(const WidgetTouchEvent& aOther,
                   CloneTouches aCloneTouches = CloneTouches::No)
      : WidgetInputEvent(aOther.IsTrusted(), aOther.mMessage, aOther.mWidget,
                         eTouchEventClass) {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mModifiers = aOther.mModifiers;
    mTimeStamp = aOther.mTimeStamp;
    if (static_cast<bool>(aCloneTouches)) {
      mTouches.SetCapacity(aOther.mTouches.Length());
      for (const RefPtr<dom::Touch>& touch : aOther.mTouches) {
        RefPtr<dom::Touch> clonedTouch = new dom::Touch(*touch);
        mTouches.AppendElement(std::move(clonedTouch));
      }
    } else {
      mTouches.AppendElements(aOther.mTouches);
    }
    mInputSource = aOther.mInputSource;
    mButton = aOther.mButton;
    mButtons = aOther.mButtons;
    mFlags.mCancelable = mMessage != eTouchCancel;
    mFlags.mHandledByAPZ = aOther.mFlags.mHandledByAPZ;
  }

  WidgetTouchEvent(WidgetTouchEvent&& aOther)
      : WidgetInputEvent(std::move(aOther)) {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mModifiers = aOther.mModifiers;
    mTimeStamp = aOther.mTimeStamp;
    mTouches = std::move(aOther.mTouches);
    mInputSource = aOther.mInputSource;
    mButton = aOther.mButton;
    mButtons = aOther.mButtons;
    mFlags = aOther.mFlags;
  }

  WidgetTouchEvent& operator=(WidgetTouchEvent&&) = default;

  WidgetTouchEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget,
                   const WidgetEventTime* aTime = nullptr)
      : WidgetInputEvent(aIsTrusted, aMessage, aWidget, eTouchEventClass,
                         aTime) {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mFlags.mCancelable = mMessage != eTouchCancel;
  }

  MOZ_COUNTED_DTOR_OVERRIDE(WidgetTouchEvent)

  WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eTouchEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetTouchEvent* result =
        new WidgetTouchEvent(false, mMessage, nullptr, this);
    result->AssignTouchEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  TouchArray mTouches;
  uint16_t mInputSource = 5;  // MouseEvent_Binding::MOZ_SOURCE_TOUCH
  int16_t mButton = eNotPressed;
  int16_t mButtons = 0;

  void AssignTouchEventData(const WidgetTouchEvent& aEvent, bool aCopyTargets) {
    AssignInputEventData(aEvent, aCopyTargets);

    // Assign*EventData() assume that they're called only new instance.
    MOZ_ASSERT(mTouches.IsEmpty());
    mTouches.AppendElements(aEvent.mTouches);
    mInputSource = aEvent.mInputSource;
  }

  void SetConvertToPointerRawUpdate(bool aConvert) {
    for (dom::Touch* const touch : mTouches) {
      touch->convertToPointerRawUpdate = aConvert;
    }
  }
  [[nodiscard]] bool CanConvertToPointerRawUpdate() const {
    for (const dom::Touch* const touch : mTouches) {
      if (touch->convertToPointerRawUpdate) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace mozilla

#endif  // mozilla_TouchEvents_h__
