#include "PluginEditor.h"
#include "PluginProcessor.h"

RaveAPEditor::RaveAPEditor(RaveAP &p, AudioProcessorValueTreeState &vts)
    : AudioProcessorEditor(&p), ChangeListener(), _lightLookAndFeel(),
      _darkLookAndFeel(), audioProcessor(p), _avts(vts), _foldablePanel(p),
      _bgFull(ImageCache::getFromMemory(BinaryData::bg_full_png,
                                        BinaryData::bg_full_pngSize))
  {
  String path =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getFullPathName();

  _header.setLookAndFeel(&_darkLookAndFeel);
  _modelPanel.setLookAndFeel(&_darkLookAndFeel);
  _modelExplorer.setLookAndFeel(&_lightLookAndFeel);
  _foldablePanel.setLookAndFeel(&_lightLookAndFeel);

  _modelDownloadPool = std::make_unique<ThreadPool>(1);

  _modelExplorer._downloadButton.onClick = [this]() {
    downloadModel(_modelExplorer._modelsList.getSelectedRow());
      // std::tuple<URL, File> model_infos;
      // try {
      //   _modelExplorer.displayDownload();
      //   model_infos = audioProcessor.getFileForDownload(_modelExplorer._modelsList.getSelectedRow());
      // } catch (const std::runtime_error& e) {
      //   AlertWindow::showAsync(MessageBoxOptions()
      //                       .withIconType(MessageBoxIconType::WarningIcon)
      //                       .withTitle("Network Warning:")
      //                       .withMessage("Could not retrieve information for target model")
      //                       .withButton("OK"),
      //                       nullptr);
      //   return;
      // }
      // auto url = std::get<0>(model_infos);
      // auto outputFile = std::get<1>(model_infos);

      // if (hasFailed) {
      //   std::cerr << "[-] Network - Failed to download model" << std::endl;
      //   AlertWindow::showAsync(MessageBoxOptions()
      //                              .withIconType(MessageBoxIconType::WarningIcon)
      //                              .withTitle("Network Warning:")
      //                              .withMessage("Failed to download model")
      //                              .withButton("OK"),
      //                               nullptr);
      //   return;
      // }
      // while (!res->isFinished())
      //   Thread::sleep(500); 
      // audioProcessor.updateAvailableModels(); 
      // updateModelComboList();
  };
  _modelExplorer._importButton.onClick = [this]() { importModel(); };

  _modelPanel.setSampleRate(p.getSampleRate());

  // Model manager button stuff
   updateModelComboList();
   updateModelExplorer();
  _header._modelComboBox.onChange = [this]() {
    audioProcessor.loadModel(_header._modelComboBox.getText());
  };

  _header.connectVTS(vts);
  _modelPanel.connectVTS(vts);
  _foldablePanel.connectVTS(vts);

  // link to model
  p._rave->addChangeListener(this);
  _modelPanel.setModel(p._rave.get());

  // Model manager button stuff
  _header._modelManagerButton.onClick = [this]() {
    if (_header._modelManagerButton.getToggleState()) {
      _header._modelManagerButton.setButtonText("Play");
      _modelPanel.setVisible(false);
      _foldablePanel.setVisible(false);
      _modelExplorer.setVisible(true);
    } else {
      // Go into play mode
      _header._modelManagerButton.setButtonText("Model Explorer");
      _modelPanel.setVisible(true);
      _foldablePanel.setVisible(true);
      _modelExplorer.setVisible(false);
    }
  };

  addAndMakeVisible(_header);
  addAndMakeVisible(_modelPanel);
  addAndMakeVisible(_foldablePanel);
  addAndMakeVisible(_console);
  addChildComponent(_modelExplorer);

  setResizable(false, false);
  getConstrainer()->setMinimumSize(996, 560);
  setSize(996, 560);
}

void RaveAPEditor::updateModelComboList() {
   MessageManagerLock mml (Thread::getCurrentThread());
   while (!mml.lockWasGained()) { Thread::sleep(10); }
  _header._modelComboBox.clear();
  auto availableModels = audioProcessor.getAvailableModels();
  if (availableModels.size() == 0) {
    _header._modelComboBox.addItem(String("-- no model available --"), 1);
    _header._modelComboBox.setItemEnabled(1, false);
  } else {
    _header._modelComboBox.addItemList(availableModels, 1);
    auto modelIdx = audioProcessor.getCurrentModelIdx();
    if ((audioProcessor.isModelAvailable()) && (modelIdx > -1)) {
      _header._modelComboBox.setSelectedItemIndex(audioProcessor.getCurrentModelIdx(), NotificationType::dontSendNotification);
    } else {
      String emptyItemString;
      if (audioProcessor.getCurrentModelName() == "") {
        emptyItemString = String("-- no model --");
        _header._modelComboBox.setText(emptyItemString);
      } else {
        emptyItemString = audioProcessor.getCurrentModelName();
        _header._modelComboBox.addItem(emptyItemString, availableModels.size() + 1);
        _header._modelComboBox.setSelectedItemIndex(availableModels.size(), NotificationType::dontSendNotification);
        _header._modelComboBox.setItemEnabled(availableModels.size()+1, false);
      }
    }
  }
}

void RaveAPEditor::updateModelExplorer() {
  auto apiModelInfo = audioProcessor.getAvailableModelsFromAPI();
  _modelExplorer._ApiModelsNames.clear();
  _modelExplorer._ApiModelsData.clear();
  for (const auto& [name, json, props] : apiModelInfo) {
    _modelExplorer._ApiModelsNames.add(name);
    _modelExplorer._ApiModelsData.add(props);
  }
  _modelExplorer._modelsList.updateContent();
}

RaveAPEditor::~RaveAPEditor() {
  this->audioProcessor._rave->removeChangeListener(this);
}

void RaveAPEditor::importModel() {
  _fc.reset(new FileChooser(
      "Choose your model file",
      File::getSpecialLocation(File::SpecialLocationType::userHomeDirectory),
      "*.ts", true));

  _fc->launchAsync(
      FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
      [this](const FileChooser &chooser) {
        File sourceFile;
        auto results = chooser.getURLResults();

        for (const URL result : results) {
          if (result.isLocalFile()) {
            sourceFile = result.getLocalFile();
          } else {
            return;
          }
        }
        if (sourceFile.getFileExtension() == ".ts" &&
            sourceFile.getSize() > 0) {
          sourceFile.copyFileTo(File(audioProcessor.getModelsDirPath()).getNonexistentChildFile(
              sourceFile.getFileName(), ".ts"));
          audioProcessor.updateAvailableModels();
        }
      });
}


void RaveAPEditor::resized() {
  // Child components should not handle margins, do it here
  // Setup the header first
  auto b_area = getLocalBounds().reduced(UI_MARGIN_SIZE);
  _header.setBounds(b_area.removeFromTop(UI_MARGIN_SIZE * 3));
  b_area.removeFromTop(UI_MARGIN_SIZE);

  // Now setup the two main bodies (explorer and viz)
  auto columnWidth = (b_area.getWidth() - (UI_MARGIN_SIZE * 3)) / 4;
  // ModelExplorer is full area, as there is no FoldablePanel here
  _modelExplorer.setBounds(b_area);
  // The model panel however only have the 3 leftmost columns
  _modelPanel.setBounds(
      b_area.removeFromLeft(columnWidth * 3 + (UI_MARGIN_SIZE * 2)));

  // Reset the b_area, as we don't want the right margin
  // because the FoldablePanel must touch the right window border
  b_area = getLocalBounds()
               .withTrimmedTop(UI_MARGIN_SIZE * 5) // Header and top margins
               .withTrimmedBottom(UI_MARGIN_SIZE);
  _foldablePanel.setBounds(
      b_area.removeFromRight(columnWidth + UI_MARGIN_SIZE));
  _console.setBounds(0, getLocalBounds().getHeight() - 20,
                     getLocalBounds().getWidth(), 20);
}

void RaveAPEditor::paint(juce::Graphics &g) {
  g.fillAll(Colour::fromRGBA(185, 185, 185, 255));
  g.drawImageAt(_bgFull, 0, 0);
}

void RaveAPEditor::log(String /*str*/) {}

void RaveAPEditor::changeListenerCallback(ChangeBroadcaster * /*source*/) {
  if (audioProcessor._rave != nullptr) {
    _foldablePanel.setBufferSizeRange(
        audioProcessor._rave->getValidBufferSizes());
    _modelPanel.setPriorEnabled(audioProcessor._rave->hasPrior());
  }
}

void RaveAPEditor::downloadModel(const int apiModelIdx) {
  auto model_infos = audioProcessor.getFileForDownload(apiModelIdx);
  auto [url, outputPath] = model_infos;
  juce::ScopedLock downloadLock(_modelDownloadMutex);
  _modelDownloadPool->addJob(new ModelDownloadJob(audioProcessor, *this, url, outputPath), true);
}

void RaveAPEditor::finished(URL::DownloadTask *task, bool success) {
  
  if (success) {
    std::cout << "[+] Network - Model downloaded" << std::endl;
    // AlertWindow::showAsync(MessageBoxOptions()
    //                            .withIconType(MessageBoxIconType::InfoIcon)
    //                            .withTitle("Information:")
    //                            .withMessage("Model successfully downloaded")
    //                            .withButton("OK"),
    //                        nullptr);
    audioProcessor.updateAvailableModels();
    updateModelComboList();
  } else {
    std::cerr << "[-] Network - Failed to download model" << std::endl;
    // AlertWindow::showAsync(MessageBoxOptions()
    //                            .withIconType(MessageBoxIconType::WarningIcon)
    //                            .withTitle("Network Warning:")
    //                            .withMessage("Failed to download model")
    //                            .withButton("OK"),
    //                        nullptr);
  }
}

void RaveAPEditor::progress(URL::DownloadTask *task, int64 bytesDownloaded,
                            int64 totalLength) {
  _modelExplorer.setDownloadProgress((double)bytesDownloaded / (double)totalLength);
  // std::cout << bytesDownloaded << "/" << totalLength << '\n';
}