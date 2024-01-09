#include "PluginProcessor.h"
#include "helpers.h"

std::vector<std::tuple<String, DynamicObject, NamedValueSet>> RaveAP::getAvailableModelsFromAPI() const {
    return _availableModelsAPI;
}

void RaveAP::updateModelsDirPath() {
  _modelsDirPath = retrieveModelsDirPath();
}

void RaveAP::updateAvailableModels() {
  _availableModels = retrieveAvailableModels(_modelsDirPath);
}

String RaveAP::getPathFromModel(String model) {
  int modelIdx = getAvailableModels().indexOf(model);
  if (modelIdx == -1) {
    throw std::runtime_error("model not present in available models : " + model.toStdString());
  }
  return getAvailableModelsPath()[modelIdx];
}

void RaveAP::updateAvailableModelsFromAPI() {
  if (_apiRoot.length() == 0) {
    _apiRoot = getApiRoot();
  }
  URL url(_apiRoot + String("get_available_models"));
  String res = url.readEntireTextStream();

  if (res.length() > 0) {
    std::cout << "[ ] Network - Received API response" << std::endl;
  } else {
    std::cerr << "[-] Network - No API response" << std::endl;
    return;
  }

  if (JSON::parse(res, _parsedJson).wasOk()) {
    Array<juce::var> &available_models =
        *_parsedJson["available_models"].getArray();
    std::cout << "[+] Network - Successfully parsed JSON, "
              << available_models.size() << " models available online:" << '\n';
    for (int i = 0; i < available_models.size(); i++) {
      // Add model variation name to its array
      String modelName = available_models[i].toString();
      //   std::cout << "\t- " << modelName << '\n';
      //   _modelExplorer._ApiModelsNames.add(modelName);
      // Add all the model variation data to its own array
      DynamicObject modelVariationData =
          *_parsedJson[modelName.toStdString().c_str()].getDynamicObject();
      NamedValueSet props = modelVariationData.getProperties();
      auto modelData = std::make_tuple(modelName, modelVariationData, props);
      _availableModelsAPI.push_back(modelData);
    }
  }
}


std::tuple<URL, File> RaveAP::getFileForDownload(int apiModelIdx) {
  if (_availableModelsAPI.size() < 1) {
    std::cout << "[ ] Network - No models available for download" << std::endl;
    // AlertWindow::showAsync(MessageBoxOptions()
    //                            .withIconType(MessageBoxIconType::WarningIcon)
    //                            .withTitle("Information:")
    //                            .withMessage("No models available for
    //                            download") .withButton("OK"),
    //                        nullptr);
    throw std::runtime_error("No models available for download");
  }
  String modelName = std::get<0>(_availableModelsAPI[apiModelIdx]);
  String tmp_url = _apiRoot + String("get_model?model_name=") +
                   URL::addEscapeChars(modelName, true, false);
  auto url = URL(tmp_url);
  std::cout << url.toString(true) << '\n';

  // TODO: Clean up the path handling, avoid using String concatenation for
  // that
  String outputFilePath = File(_modelsDirPath).getFullPathName() + String("/") +
                          modelName + String(".ts");
  auto outputFile = File(outputFilePath);
  return std::make_tuple(url, outputFile);

}

void RaveAP::loadModel(const String modelName)
{
  if (getAvailableModels().contains(modelName)) {
    _isModelAvailable = true;
    // _avts.getParameter(rave_parameters::current_model) = modelIdx;
    updateEngine(modelName);
  } else {
    // TODO : error dialog
    std::cout << "Model " << modelName.toStdString() << " is not available." << std::endl;
    _isModelAvailable = false;
  }
  return;
}

void RaveAP::updateEngine(const String modelName) {
  if (modelName == _loadedModelName)
    return;
  _loadedModelName = modelName;
  auto modelIdx = getAvailableModels().indexOf(modelName);  
  auto modelFile = getAvailableModelsPath()[modelIdx].toStdString();
  juce::ScopedLock irCalculationlock(_engineUpdateMutex);
  if (_engineThreadPool) {
    _engineThreadPool->removeAllJobs(true, 100);
  }
  _engineThreadPool->addJob(new UpdateEngineJob(*this, modelFile), true);
}


String getModelFromState(const ValueTree &state)  {
  if (state.hasProperty(Identifier("MODEL"))) {
    return state.getProperty(Identifier("MODEL"));
  } else {
    // throw std::runtime_error("current_model property not present in state information.");
    return NO_MODEL_STRING;
  }
}

void setModelState(ValueTree &state, const String &model)  {
  state.setProperty(Identifier("MODEL"), var(model), nullptr);
}


void RaveAP::getStateInformation(juce::MemoryBlock &destData) {
  auto state = _avts.copyState();
  if (_loadedModelName != NO_MODEL_STRING) {
    setModelState(state, _loadedModelName);
  }
  std::unique_ptr<XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}


void RaveAP::setStateInformation(const void *data, int sizeInBytes) {
  std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr) {
    if (xmlState->hasTagName(_avts.state.getType())) {
      auto _importedState = ValueTree::fromXml(*xmlState);
      // check model in imported state
      auto availableModels = getAvailableModels();
      auto _modelInState =  getModelFromState(_importedState);
      std::cout << "model in state : " << _modelInState << std::endl;
      if (_modelInState != NO_MODEL_STRING) {
        // Model has been initialized. Check if model is available
        if (availableModels.contains(_modelInState)) 
        {
          // model is available ; load it
          loadModel(_modelInState);
        } else {
          // model is unvailable; bypass
          //TODO dialog asking either to download the model from the API.
          _isModelAvailable = false;
        }
      } else {
        if (availableModels.size() > 0) {
          // load first model in list
          setModelState(_importedState, availableModels[0]);
          loadModel(availableModels[0]);
          _isModelAvailable = true;
        } else {
          // no model available ; needs download / import
          _isModelAvailable = false;
        }
      }
      _avts.replaceState(_importedState);
    }
  }
}






String RaveAP::getApiRoot() {
  unsigned char b[] = {104, 116, 116, 112, 115, 58,  47,  47, 112, 108, 97,
                       121, 46,  102, 111, 114, 117, 109, 46, 105, 114, 99,
                       97,  109, 46,  102, 114, 47,  114, 97, 118, 101, 45,
                       118, 115, 116, 45,  97,  112, 105, 47};
  char c[sizeof(b) + 1];
  memcpy(c, b, sizeof(b));
  c[sizeof(b)] = '\0';
  return String(c);
}
