/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioParam.h"

#include "AudioContext.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "mozilla/dom/AudioParamBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AudioParam)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioParam)
  tmp->DisconnectFromGraphAndDestroyTrack();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioParam)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(AudioParam)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(AudioParam)

AudioParam::AudioParam(AudioNode* aNode, uint32_t aIndex,
                       const nsAString& aName, float aDefaultValue,
                       float aMinValue, float aMaxValue)
    : AudioParamTimeline(aDefaultValue),
      mNode(aNode),
      mName(aName),
      mIndex(aIndex),
      mDefaultValue(aDefaultValue),
      mMinValue(aMinValue),
      mMaxValue(aMaxValue) {}

AudioParam::~AudioParam() { DisconnectFromGraphAndDestroyTrack(); }

JSObject* AudioParam::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return AudioParam_Binding::Wrap(aCx, this, aGivenProto);
}

void AudioParam::DisconnectFromGraphAndDestroyTrack() {
  MOZ_ASSERT(mRefCnt.get() > mInputNodes.Length(),
             "Caller should be holding a reference or have called "
             "mRefCnt.stabilizeForDeletion()");

  while (!mInputNodes.IsEmpty()) {
    RefPtr<AudioNode> input = mInputNodes.PopLastElement().mInputNode;
    input->RemoveOutputParam(this);
  }

  if (mNodeTrackPort) {
    mNodeTrackPort->Destroy();
    mNodeTrackPort = nullptr;
  }

  if (mTrack) {
    mTrack->Destroy();
    mTrack = nullptr;
  }
}

mozilla::MediaTrack* AudioParam::GetTrack() const { return mTrack; }

mozilla::MediaTrack* AudioParam::Track() {
  if (mTrack) {
    return mTrack;
  }

  AudioNodeEngine* engine = new AudioNodeEngine(nullptr);
  mTrack = AudioNodeTrack::Create(mNode->Context(), engine,
                                  AudioNodeTrack::NO_TRACK_FLAGS,
                                  mNode->Context()->Graph());

  // Force the input to have only one channel, and make it down-mix using
  // the speaker rules if needed.
  mTrack->SetChannelMixingParametersImpl(1, ChannelCountMode::Explicit,
                                         ChannelInterpretation::Speakers);
  // Mark as an AudioParam helper track
  mTrack->SetAudioParamHelperTrack();

  // Setup the AudioParam's track as an input to the owner AudioNode's track
  AudioNodeTrack* nodeTrack = mNode->GetTrack();
  if (nodeTrack) {
    mNodeTrackPort = nodeTrack->AllocateInputPort(mTrack);
  }

  // Send the track to the timeline on the MTG side.
  AudioParamEvent event(mTrack);
  SendEventToEngine(event);

  return mTrack;
}

void AudioParam::SendEventToEngine(const AudioParamEvent& aEvent) {
  if (WEB_AUDIO_API_LOG_TEST()) {
    nsAutoCString params;
    if (aEvent.mType == AudioTimelineEvent::SetValueCurve) {
      params.AppendFmt("length={} time={:f} duration={:f}",
                       aEvent.CurveLength(), aEvent.Time<double>(),
                       aEvent.Duration());
    } else {
      params.AppendFmt("value={} time={:f}", aEvent.NominalValue(),
                       aEvent.Time<double>());
      if (aEvent.mType == AudioTimelineEvent::SetTarget) {
        params.AppendFmt(" constant={}", aEvent.TimeConstant());
      }
    }
    WEB_AUDIO_API_LOG("%f: %s for %u %s %s", GetParentObject()->CurrentTime(),
                      NS_ConvertUTF16toUTF8(mName).get(), ParentNodeId(),
                      AudioTimelineEvent::EnumValueToString(aEvent.mType),
                      params.get());
  }
  AudioNodeTrack* track = mNode->GetTrack();
  if (track) {
    track->SendTimelineEvent(mIndex, aEvent);
  }
}

void AudioParam::CleanupOldEvents() {
  MOZ_ASSERT(NS_IsMainThread());
  double currentTime = mNode->Context()->CurrentTime();

  CleanupEventsOlderThan(currentTime);
}

float AudioParamTimeline::AudioNodeInputValue(size_t aCounter) const {
  MOZ_ASSERT(mTrack);

  // If we have a chunk produced by the AudioNode inputs to the AudioParam,
  // get its value now.  We use aCounter to tell us which frame of the last
  // AudioChunk to look at.
  float audioNodeInputValue = 0.0f;
  const AudioBlock& lastAudioNodeChunk = mTrack->LastChunks()[0];
  if (!lastAudioNodeChunk.IsNull()) {
    MOZ_ASSERT(lastAudioNodeChunk.GetDuration() == WEBAUDIO_BLOCK_SIZE);
    audioNodeInputValue =
        static_cast<const float*>(lastAudioNodeChunk.mChannelData[0])[aCounter];
    audioNodeInputValue *= lastAudioNodeChunk.mVolume;
  }

  return audioNodeInputValue;
}

}  // namespace mozilla::dom
