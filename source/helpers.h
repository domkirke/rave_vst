#pragma once
#include <JuceHeader.h>



bool pathExist(const std::string &s);
std::string capitalizeFirstLetter(std::string text);
String retrieveModelsDirPath(); 
std::tuple<StringArray, StringArray> retrieveAvailableModels(String _modelsDirPath);

// Smooth Muting enum
enum class RAVESmoothingStatus: int {ignore = 0, needsMute = 1, needsDemute = 2};


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


// Non automatisable parameters for model & latency mode

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

class NAAudioParameterChoice: public juce::AudioParameterChoice {
  public: 
    NAAudioParameterChoice(const ParameterID & 	parameterID,
                           const String & 	parameterName,
                           const StringArray & 	choicesToUse,
                           int 	defaultItemIndex,
                           const String & 	parameterLabel,
                           std::function< String(int index, int maximumStringLength)> 	stringFromIndex = nullptr,
                           std::function< int(const String &text)> indexFromString = nullptr): 
                           juce::AudioParameterChoice(parameterID, parameterName, choicesToUse, defaultItemIndex, parameterLabel, stringFromIndex, indexFromString)
    {}

    bool isAutomatable() const override { return false; }
};
