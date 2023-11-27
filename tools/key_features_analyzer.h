
#include <string>
#include "rocksdb/env.h"

namespace ROCKSDB_NAMESPACE {
class KeyFeaturesAnalyzerTool{

public:
    KeyFeaturesAnalyzerTool(std::string& key_fetures_trace_path, std::string& output_path);
    ~KeyFeaturesAnalyzerTool() {}

    Status StartProcessing();
private:
  ROCKSDB_NAMESPACE:: Env* env_;
  std::string key_fetures_trace_path_;
  std::string output_path_;
  char buffer_[1024];


};



int key_features_analyzer_main(int argc, char** argv);

}
