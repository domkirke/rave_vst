#pragma once

#include "Rave.h"
#include "CircularBuffer.h"
#include "EngineUpdater.h"
#include <JuceHeader.h>
#include <algorithm>
#include <torch/script.h>
#include <torch/torch.h>

#define EPSILON 0.0000001

const size_t AVAILABLE_DIMS = 8;
const juce::StringArray channel_modes = {"L", "R", "L + R"};

namespace rave_parameters {
const String model_selection{"model_selection"};
const String input_gain{"input_gain"};
const String channel_mode{"channel_mode"};
const String input_thresh{"input_threshold"};
const String input_ratio{"input_ratio"};
const String latent_jitter{"latent_jitter"};
const String output_width{"output_width"};
const String output_gain{"output_gain"};
const String output_limit{"output_limit"};
const String output_drywet{"ouptut_drywet"};
const String latent_scale{"latent_scale"};
const String latent_bias{"latent_bias"};
const String latency_mode{"latency_mode"};
const String use_prior{"use_prior"};
const String prior_temperature{"prior_temperature"};
const String mute_with_playback{"mute_with_playback"};
} // namespace rave_parameters


namespace rave_ranges {
  const NormalisableRange<float> gainRange(-70.f, 12.f);
  const NormalisableRange<float> latentScaleRange(0.0f, 5.0f);
  const NormalisableRange<float> latentBiasRange(-3.0f, 3.0f);
} // namespace rave_ranges


// Automatic latency handling

enum class RAVELatencyStateStatus: int { empty = 0, pending = 1, completed = 2, requested = 3 };

class RAVELatencyState {
  public: 
    RAVELatencyState() {
      _latencyTime = 0.;
      _status = RAVELatencyStateStatus::empty;
    }

    RAVELatencyStateStatus status() const {
      return _status;
    }

    void setRequested() {
      _status = RAVELatencyStateStatus::requested;
    }

    void setEmpty() { 
      _latencyTime = 0.;
      _status = RAVELatencyStateStatus::empty; 
    }

    void startTimer() { 
      if (_status == RAVELatencyStateStatus::requested) {
        _latencyTime = 0.;
        _status = RAVELatencyStateStatus::pending;
        _start_point = std::chrono::steady_clock::now();
      } else {
        throw std::runtime_error("timer started, but latency updated is not requested");
      }
    }

    void endTimer() {
      if (_status == RAVELatencyStateStatus::pending) {
        _latencyTime = 0.;
        _status = RAVELatencyStateStatus::completed;
        auto _end_point = std::chrono::steady_clock::now();
        const std::chrono::duration<double> diff{_end_point - _start_point};
        _latencyTime = diff.count();

      } else {
        throw std::runtime_error("timer ended, but status is not pending");
      }
    }

    int getLatencySamples (double sample_rate) {
      if (_status == RAVELatencyStateStatus::completed) {
        return (int)std::ceil(_latencyTime * sample_rate);
      }
      else
      {
        throw std::runtime_error("asked for latency samples, but status is not completed");
      }
    }

  private:
    double _latencyTime;
    std::chrono::time_point<std::chrono::steady_clock> _start_point;
    RAVELatencyStateStatus _status;
};


class NAAudioParameterInt: public juce::AudioParameterInt {
  public: 
    NAAudioParameterInt(const ParameterID &parameterID,
                        const String &parameterName,
                        int minValue, int maxValue, 
                        int defaultValue, const String &parameterLabel=String(), 
                        std::function< String(int value, int maximumStringLength)> stringFromInt=nullptr,
                        std::function< int(const String &text)> intFromString=nullptr): 
                        juce::AudioParameterInt(parameterID, parameterName, minValue, maxValue, defaultValue, parameterLabel, stringFromInt, intFromString)  
    {}

    bool isAutomatable() const override { return false; }
};


class RaveAP : public juce::AudioProcessor,
               public juce::AudioProcessorValueTreeState::Listener {
  // WARNING: As we do not implement processBlock() without parameters like in:
  // https://docs.juce.com/master/classAudioProcessor.html#abbac77f68ba047cf60c4bc97326dcb58
  // we explicitely authorize the use of AudioProcessor's processBlock
  // implementation through RaveAP. See:
  // https://stackoverflow.com/questions/9995421/gcc-woverloaded-virtual-warnings
  using juce::AudioProcessor::processBlock;

public:
  RaveAP();
  ~RaveAP() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
  void modelPerform();
  void detectAvailableModels();
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;
  const juce::String getName() const override;
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;
  AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;
  void parameterChanged(const String &parameterID, float newValue) override;

  auto mute() -> void;
  auto unmute() -> void;
  auto getIsMuted() -> const bool;
  void updateBufferSizes();
  RAVELatencyState& getLatencyState() { return _latencyState; }
  void updateLatency() {
    if (_latencyState.status() == RAVELatencyStateStatus::completed) {
      int processing_latency = _latencyState.getLatencySamples(getSampleRate());
      int full_latency = processing_latency + (int)pow(2, *_latencyMode);
      setLatencySamples(full_latency);
      std::cout << "latency set to " << full_latency << std::endl; 
    } else {
      throw std::runtime_error("Got update latency, but latency chrono status is not completed.");
    }
  }
  void setLatencySamples(int samples) {
    AudioProcessor::setLatencySamples(samples);
    _dryWetMixerEffect.setWetLatency(samples);
  };

  void updateEngine(const std::string modelFile);
  std::string capitalizeFirstLetter(std::string text);
  float getAmplitude(float *buffer, size_t len);

  std::unique_ptr<RAVE> _rave;
  float _inputAmplitudeL;
  float _inputAmplitudeR;
  float _outputAmplitudeL;
  float _outputAmplitudeR;
  bool _plays = false; 

  double getSampleRate() { return _sampleRate; }

private:
  mutable CriticalSection _engineUpdateMutex;
  juce::AudioProcessorValueTreeState _avts;
  std::unique_ptr<juce::ThreadPool> _engineThreadPool;
  std::string _loadedModelName;
  RAVELatencyState _latencyState;


  /*
   *Allocate some memory to use as the circular_buffer storage
   *for each of the circular_buffer types to be created
  */
  double _sampleRate = 0;
  std::unique_ptr<circular_buffer<float, float>[]> _inBuffer;
  std::unique_ptr<circular_buffer<float, float>[]> _outBuffer;
  std::vector<std::unique_ptr<float[]>> _inModel, _outModel;
  std::unique_ptr<std::thread> _computeThread;

  bool _editorReady;

  float *_inFifoBuffer{nullptr};
  float *_outFifoBuffer{nullptr};

  std::atomic<float> *_inputGainValue;
  std::atomic<float> *_thresholdValue;
  std::atomic<float> *_ratioValue;
  std::atomic<float> *_latentJitterValue;
  std::atomic<float> *_widthValue;
  std::atomic<float> *_outputGainValue;
  std::atomic<float> *_dryWetValue;
  std::atomic<float> *_limitValue;
  std::atomic<float> *_channelMode;
  // latency mode contains the power of 2 of the current refresh rate.
  std::atomic<float> *_latencyMode;
  std::atomic<float> *_usePrior;
  std::atomic<float> *_priorTemperature;
  std::atomic<float> *_muteWithPlayback;

  std::array<std::atomic<float> *, AVAILABLE_DIMS> *_latentScale;
  std::array<std::atomic<float> *, AVAILABLE_DIMS> *_latentBias;
  std::atomic<bool> _isMuted{true};

  enum class muting : int { ignore = 0, mute, unmute };

  std::atomic<muting> _fadeScheduler{muting::mute};
  LinearSmoothedValue<float> _smoothedFadeInOut;
  LinearSmoothedValue<float> _smoothedWetGain;
  LinearSmoothedValue<float> _smoothedDryGain;

  // DSP effect
  juce::dsp::Compressor<float> _compressorEffect;
  juce::dsp::Gain<float> _inputGainEffect;
  juce::dsp::Gain<float> _outputGainEffect;
  juce::dsp::Limiter<float> _limiterEffect;
  juce::dsp::DryWetMixer<float> _dryWetMixerEffect;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RaveAP)
};
