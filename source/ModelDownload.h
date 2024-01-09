#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"


class ModelDownloadJob : public juce::ThreadPoolJob {
  public: 
    explicit ModelDownloadJob(RaveAP &processor, RaveAPEditor &editor, juce::URL url, const juce::File outputPath, const int maxAttempts=10);
    virtual ~ModelDownloadJob() = default;
    virtual auto runJob() -> JobStatus;
  private:
    RaveAP &mProcessor;
    RaveAPEditor &mEditor;
    URL _url;
    File _outputPath;
    int _attempts = 0;
    const int _maxAttempts;
    ModelDownloadJob(const ModelDownloadJob &);
    ModelDownloadJob &operator=(const ModelDownloadJob &);
};
