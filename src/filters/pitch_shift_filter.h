#ifndef PITCH_SHIFT_FILTER_H
#define PITCH_SHIFT_FILTER_H

#include "../soloud/include/soloud.h"
#include "../soundtouch/SoundTouch.h"
#include "../signalsmith/signalsmith-stretch.h"

#include <vector>

class PitchShift;

enum class PitchShiftAlgorithm { Signalsmith = 0, SoundTouch = 1 };

class PitchShiftInstance : public SoLoud::FilterInstance {
  struct ChannelState {
    soundtouch::SoundTouch shifter;
    std::vector<float> outputQueue;
    std::vector<float> dryBuffer;
    std::vector<float> processedBuffer;
    std::vector<float> pullBuffer;
    double lastPitch = 1.0;
    unsigned int sampleRate = 0;
    bool configured = false;
  };

  struct SignalsmithState {
    signalsmith::stretch::SignalsmithStretch<float> stretcher;
    std::vector<float *> inputPtrs;
    std::vector<float *> outputPtrs;
    double lastPitch = 1.0;
    unsigned int sampleRate = 0;
    unsigned int channelCount = 0;
    bool configured = false;
  };

  std::vector<ChannelState> mChannelStates;
  SignalsmithState mSignalsmithState;
  PitchShift *mParent;
  unsigned int mChannelCount = 0;
  PitchShiftAlgorithm mActiveAlgorithm = PitchShiftAlgorithm::Signalsmith;

public:
  virtual void filter(float *aBuffer, unsigned int aSamples,
                      unsigned int aBufferSize, unsigned int aChannels,
                      float aSamplerate, SoLoud::time aTime);
  PitchShiftInstance(PitchShift *aParent);
  void setFilterParameter(unsigned int aAttributeId, float aValue);
};

class PitchShift : public SoLoud::Filter {

public:
  enum FILTERATTRIBUTE { WET = 0, SHIFT = 1, SEMITONES = 2, ALGORITHM = 3 };
  float mWet;
  float mShift;
  float mSemitones;
  float mAlgorithm;
  virtual int getParamCount();
  virtual const char *getParamName(unsigned int aParamIndex);
  virtual unsigned int getParamType(unsigned int aParamIndex);
  virtual float getParamMax(unsigned int aParamIndex);
  virtual float getParamMin(unsigned int aParamIndex);
  SoLoud::result setParam(unsigned int aParamIndex, float aValue);
  virtual SoLoud::FilterInstance *createInstance();
  PitchShift();
};

#endif
