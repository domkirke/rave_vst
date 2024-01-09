#pragma once

#include "Rave.h"
#include "helpers.h"
#include "CircularBuffer.h"
#include "EngineUpdater.h"
#include <JuceHeader.h>
#include <algorithm>
#include <torch/script.h>
#include <torch/torch.h>
#include <sys/stat.h>

#define EPSILON 0.0000001

const size_t AVAILABLE_DIMS = 8;
const String NO_MODEL_STRING = "";
const juce::StringArray channel_modes = {"L", "R", "L + R"};

namespace rave_parameters {
const String current_model{"current_model"};
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
  AudioProcessorValueTreeState::ParameterLayout createParameterLayout(const StringArray modelNamesList);
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
    _dryWetMixerEffect.setWetLatency(static_cast<float>(samples));
  }

  std::string capitalizeFirstLetter(std::string text);
  float getAmplitude(float *buffer, size_t len);

  std::unique_ptr<RAVE> _rave;
  // getter functions for models
  String getModelsDirPath() const {return _modelsDirPath; }
  StringArray getAvailableModels() {return std::get<0>(_availableModels); }
  StringArray getAvailableModelsPath() {return std::get<1>(_availableModels); }
  String getPathFromModel(String model);
  // update functions for models
  void updateModelsDirPath();
  void updateAvailableModels();
  void updateAvailableModelsFromAPI();
  std::tuple<URL, File> getFileForDownload(int apiModelIdx);
  std::vector<std::tuple<String, DynamicObject, NamedValueSet>> getAvailableModelsFromAPI() const;
  void loadModel(String modelName);
  bool isModelAvailable() const {return _isModelAvailable;}; // For a special case when model is not present but loaded in AVTS
  int getCurrentModelIdx() {
    return getAvailableModels().indexOf(_loadedModelName);
  }
  String getCurrentModelName() const {return _loadedModelName; }
  juce::String getApiRoot();

  float _inputAmplitudeL;
  float _inputAmplitudeR;
  float _outputAmplitudeL;
  float _outputAmplitudeR;
  bool _plays = false; 

  double getSampleRate() const { return _sampleRate; }

private:
  mutable CriticalSection _engineUpdateMutex;
  juce::AudioProcessorValueTreeState _avts;
  std::unique_ptr<juce::ThreadPool> _engineThreadPool;
  String _loadedModelName = NO_MODEL_STRING;
  bool _isModelAvailable = true;
  RAVELatencyState _latencyState;
  void updateEngine(const String modelName);

  /* Model handling */
  juce::String _modelsDirPath;
  std::tuple<StringArray, StringArray> _availableModels;
  std::vector<std::tuple<String, DynamicObject, NamedValueSet>> _availableModelsAPI;
  juce::var _parsedJson;
  juce::String _apiRoot;

  /*
   *Allocate some memory to use as the circular_buffer storage
   *for each of the circular_buffer types to be created
  */
  double _sampleRate = 0;
  double _hostSampleRate = 0;

  void setRateAndBufferSizeDetails(double sampleRate, int blockSize) {
    _hostSampleRate = sampleRate;
}
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
  RAVESmoothingStatus _smoothStatus = RAVESmoothingStatus::ignore;
  int _smoothingSamples = 20;

  // DSP effect
  juce::dsp::Compressor<float> _compressorEffect;
  juce::dsp::Gain<float> _inputGainEffect;
  juce::dsp::Gain<float> _outputGainEffect;
  juce::dsp::Limiter<float> _limiterEffect;
  juce::dsp::DryWetMixer<float> _dryWetMixerEffect;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RaveAP)
};

