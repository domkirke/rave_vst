#include "helpers.h" 
#include <sys/types.h>
#include <sys/stat.h>
#include <filesystem>

bool pathExist(const std::string &s) {
  struct stat buffer;
  return (stat(s.c_str(), &buffer) == 0);
}

std::string capitalizeFirstLetter(std::string text) {
  for (unsigned int x = 0; x < text.length(); x++) {
    if (x == 0) {
      text[x] = toupper(text[x]);
    } else if (text[x - 1] == ' ') {
      text[x] = toupper(text[x]);
    }
  }
  return text;
}

String retrieveModelsDirPath() { 
  String path =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getFullPathName();

  if (SystemStats::getOperatingSystemType() ==
      SystemStats::OperatingSystemType::MacOSX)
    path += String("/Application Support");
  path += String("/ACIDS/RAVE/");
  
  if (auto modelsDirPathFile = File(path); modelsDirPathFile.isDirectory() == false) {
    modelsDirPathFile.createDirectory();
  }
  return path;
}


std::tuple<StringArray, StringArray> retrieveAvailableModels(String pathStr) {
  std::string ext(".ts");
  auto path = File(pathStr);
  auto availableModels = StringArray();
  auto availableModelsPaths = StringArray();
  // Reset list of available models
  if (!pathExist(path.getFullPathName().toStdString())) {
    throw std::runtime_error("Model directory not found.");
  }
  try {
    for (auto &p : std::filesystem::recursive_directory_iterator(
             path.getFullPathName().toStdString())) {
      if (p.path().extension() == ext) {
        availableModelsPaths.add(String(p.path().string()));
        // Prepare a clean model name for display
        auto tmpModelName = p.path().stem().string();
        std::replace(tmpModelName.begin(), tmpModelName.end(), '_', ' ');
        tmpModelName = capitalizeFirstLetter(tmpModelName);
        availableModels.add(tmpModelName);
      }
    }
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "[-] - Model not found" << '\n';
    std::cerr << e.what();
    throw std::runtime_error("Model not found.");
  }
  return std::make_tuple(availableModels, availableModelsPaths);
}
