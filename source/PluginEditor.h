#pragma once
#include "PluginProcessor.h"
#include "ui/FoldablePanel.h"
#include "ui/GUI_GLOBALS.h"
#include "ui/Header.h"
#include "ui/ModelExplorer.h"
#include "ui/ModelPanel.h"
#include "ui/MyLookAndFeel.h"
#include <sys/stat.h>

#include <JuceHeader.h>

// using namespace juce;

class RaveAPEditor : public juce::AudioProcessorEditor,
                     public juce::ChangeListener,
                     public juce::URL::DownloadTaskListener {
public:
  RaveAPEditor(RaveAP &, AudioProcessorValueTreeState &);
  ~RaveAPEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void log(String str);
  void changeListenerCallback(ChangeBroadcaster *source) override;
  void downloadModel(const int apiModelIdx);
  ModelExplorer& getModelExplorer() {return _modelExplorer; };

  void finished(URL::DownloadTask *task, bool success) override;
  void progress(URL::DownloadTask *task, int64 bytesDownloaded,
                int64 totalLength) override;

private:
  void importModel();
  std::unique_ptr<FileChooser> _fc;
  void updateModelExplorer();
  void updateModelComboList();

  LightLookAndFeel _lightLookAndFeel;
  DarkLookAndFeel _darkLookAndFeel;
  RaveAP &audioProcessor;
  AudioProcessorValueTreeState &_avts;
  mutable CriticalSection _modelDownloadMutex;
  std::unique_ptr<juce::ThreadPool> _modelDownloadPool;

  // Main window
  ModelPanel _modelPanel;
  Header _header;
  FoldablePanel _foldablePanel;
  // Model Manager window
  ModelExplorer _modelExplorer;
  Label _console;

  Image _bgFull;

  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

  static size_t writeToFileCallback(void *ptr, size_t size, size_t nmemb,
                                    FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RaveAPEditor)
};

#include "ModelDownload.h"