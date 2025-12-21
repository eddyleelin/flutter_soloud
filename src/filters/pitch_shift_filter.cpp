#include <cmath>

#include "pitch_shift_filter.h"
#include <common.h>

#include <algorithm>
#include <vector>

PitchShiftInstance::PitchShiftInstance(PitchShift *aParent) {
  mParent = aParent;
  initParams(3);
  mParam[PitchShift::SHIFT] = aParent->mShift;
  mParam[PitchShift::SEMITONES] = aParent->mSemitones;
}

void PitchShiftInstance::filter(float *aBuffer, unsigned int aSamples,
                                unsigned int aBufferSize,
                                unsigned int aChannels, float aSamplerate,
                                SoLoud::time aTime) {
  updateParams(aTime);

  if (mChannelCount != aChannels) {
    mChannelCount = aChannels;
    mChannelStates.clear();
    mChannelStates.resize(aChannels);
  }

  const float dryRatio = 1.0f - mParam[PitchShift::WET];
  const float wetRatio = mParam[PitchShift::WET];
  const double targetPitch = mParam[PitchShift::SHIFT];
  const unsigned int sampleRate =
      static_cast<unsigned int>(aSamplerate + 0.5f);

  for (unsigned int ch = 0; ch < aChannels; ch++) {
    float *channelData = aBuffer + ch * aSamples;
    auto &state = mChannelStates[ch];

    if (!state.configured || state.sampleRate != sampleRate) {
      state.shifter.clear();
      state.shifter.setSampleRate(sampleRate);
      state.shifter.setChannels(1);
      state.shifter.setTempo(1.0);
      state.shifter.setRate(1.0);
      state.shifter.setPitch(targetPitch);
      state.sampleRate = sampleRate;
      state.lastPitch = targetPitch;
      state.outputQueue.clear();
      state.configured = true;
    } else if (std::abs(state.lastPitch - targetPitch) > 1e-4) {
      state.shifter.clear();
      state.shifter.setPitch(targetPitch);
      state.lastPitch = targetPitch;
      state.outputQueue.clear();
    }

    if (state.dryBuffer.size() < aSamples) {
      state.dryBuffer.resize(aSamples);
      state.processedBuffer.resize(aSamples);
      state.pullBuffer.resize(aSamples);
    }

    std::copy(channelData, channelData + aSamples, state.dryBuffer.begin());

    state.shifter.putSamples(channelData, aSamples);

    while (state.outputQueue.size() < aSamples) {
      const auto available = state.shifter.numSamples();
      if (available == 0) {
        break;
      }
      const auto needed =
          static_cast<uint>(aSamples - state.outputQueue.size());
      const auto toPull = std::min<uint>(available, needed);
      const auto pulled =
          state.shifter.receiveSamples(state.pullBuffer.data(), toPull);
      if (pulled == 0) {
        break;
      }
      state.outputQueue.insert(state.outputQueue.end(),
                               state.pullBuffer.begin(),
                               state.pullBuffer.begin() + pulled);
    }

    const auto fromQueue =
        std::min<size_t>(state.outputQueue.size(), aSamples);
    if (fromQueue > 0) {
      std::copy(state.outputQueue.begin(),
                state.outputQueue.begin() + fromQueue,
                state.processedBuffer.begin());
      state.outputQueue.erase(state.outputQueue.begin(),
                              state.outputQueue.begin() + fromQueue);
    }
    if (fromQueue < aSamples) {
      std::copy(state.dryBuffer.begin() + fromQueue,
                state.dryBuffer.begin() + aSamples,
                state.processedBuffer.begin() + fromQueue);
    }

    if (wetRatio >= 1.0f) {
      std::copy(state.processedBuffer.begin(),
                state.processedBuffer.begin() + aSamples, channelData);
    } else if (wetRatio <= 0.0f) {
      std::copy(state.dryBuffer.begin(),
                state.dryBuffer.begin() + aSamples, channelData);
    } else {
      for (unsigned int j = 0; j < aSamples; j++) {
        channelData[j] =
            state.dryBuffer[j] * dryRatio +
            state.processedBuffer[j] * wetRatio;
      }
    }
  }
}

void PitchShiftInstance::setFilterParameter(unsigned int aAttributeId,
                                            float aValue) {
  if (aAttributeId >= mNumParams)
    return;

  mParamFader[aAttributeId].mActive = 0;

  switch (aAttributeId) {
  case PitchShift::WET:
    if (aValue < 0.f || aValue > 1.f)
      break;
    mParam[PitchShift::WET] = aValue;
    break;
  case PitchShift::SHIFT:
    if (aValue < mParent->getParamMin(PitchShift::SHIFT) ||
        aValue > mParent->getParamMax(PitchShift::SHIFT))
      return;
    mParam[PitchShift::SHIFT] = aValue;
    mParam[PitchShift::SEMITONES] = 12 * log2f(aValue);
    break;
  case PitchShift::SEMITONES:
    if (aValue < mParent->getParamMin(PitchShift::SEMITONES) ||
        aValue > mParent->getParamMax(PitchShift::SEMITONES))
      return;
    mParam[PitchShift::SEMITONES] = aValue;
    mParam[PitchShift::SHIFT] = pow(2., aValue / 12.);
    break;
  }

  mParamChanged |= 1 << aAttributeId;
}

SoLoud::result PitchShift::setParam(unsigned int aParamIndex, float aValue) {
  switch (aParamIndex) {
  case WET:
    if (aValue < 0.f || aValue > 1.f)
      break;
    mWet = aValue;
    break;
  case SHIFT:
    if (aValue < getParamMin(SHIFT) || aValue > getParamMax(SHIFT))
      return SoLoud::INVALID_PARAMETER;
    mShift = aValue;
    mSemitones = 12 * log2f(mShift);
    break;
  case SEMITONES:
    if (aValue < getParamMin(SEMITONES) || aValue > getParamMax(SEMITONES))
      return SoLoud::INVALID_PARAMETER;
    mSemitones = aValue;
    mShift = pow(2., mSemitones / 12.);
    break;
  }
  return SoLoud::SO_NO_ERROR;
}

int PitchShift::getParamCount() { return 3; }

const char *PitchShift::getParamName(unsigned int aParamIndex) {
  switch (aParamIndex) {
  case WET:
    return "Wet";
  case SHIFT:
    return "Shift";
  case SEMITONES:
    return "Semitones";
  }
  return "Wet";
}

unsigned int PitchShift::getParamType(unsigned int aParamIndex) {
  return FLOAT_PARAM;
}

float PitchShift::getParamMax(unsigned int aParamIndex) {
  switch (aParamIndex) {
  case WET:
    return 1.f;
  case SHIFT:
    return 3.f;
  case SEMITONES:
    return 36.f;
  }
  return 1;
}

float PitchShift::getParamMin(unsigned int aParamIndex) {
  switch (aParamIndex) {
  case WET:
    return 0.f;
  case SHIFT:
    return 0.1f;
  case SEMITONES:
    return -36.f;
  }
  return 1;
}

PitchShift::PitchShift() {
  mWet = 1.0f;
  mShift = 1.0f;
  mSemitones = 0.0f;
}

SoLoud::FilterInstance *PitchShift::createInstance() {
  return new PitchShiftInstance(this);
}
