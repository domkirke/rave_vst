#include "ModelDownload.h"

// Model Download Job
ModelDownloadJob::ModelDownloadJob(RaveAP &processor, RaveAPEditor &editor, juce::URL url, const juce::File outputPath, int maxAttempts):
  ThreadPoolJob("ModelDownloadJob"), mProcessor(processor), mEditor(editor), _url(url), _outputPath(outputPath), _maxAttempts(maxAttempts)
{}

auto ModelDownloadJob::runJob() -> JobStatus {
  if (shouldExit()) {
    return JobStatus::jobNeedsRunningAgain;
  }
  // download model
  mEditor.getModelExplorer().displayDownload(_outputPath.getFileName());
  std::unique_ptr<URL::DownloadTask> res = _url.downloadToFile(_outputPath, URL::DownloadTaskOptions().withListener(&mEditor));
  bool hasFailed = (res == nullptr);
  if (!hasFailed) 
    hasFailed = res->hadError();
  if (hasFailed) {
    _attempts++;
    if (_attempts >= _maxAttempts)
      return JobStatus::jobHasFinished;
    else 
      return JobStatus::jobNeedsRunningAgain;
  }
  while (!res->isFinished())
    Thread::sleep(200); 
  
  mEditor.getModelExplorer().hideDownload();
  return JobStatus::jobHasFinished;
}
