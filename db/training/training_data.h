
#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <sstream>
#include <utility>
#include <vector>


#include "rocksdb/key_meta.h"
#include "rocksdb/status.h"
#include "memory/allocator.h"
#include "memory/concurrent_arena.h"
#include "LightGBM/c_api.h"

namespace ROCKSDB_NAMESPACE {

class Arena;

extern  std::vector<uint64_t> LifetimeSequence; 
class TrainingData{
public:
  TrainingData(Arena* arena, size_t num_features, 
               uint64_t batch_size,
               size_t num_labels=2);
  Status TrainModel(BoosterHandle* new_model,  const std::unordered_map<std::string, std::string>& training_params ); 
  // Status AddTrainingSample()

  Status ConvertLabels(uint64_t threshold);
  Status AddTrainingSample(const std::vector<double>& data, const double& label ) ;
  Status AddTrainingSample(const std::vector<uint64_t>& past_distance,
                           const uint64_t& blob_size,
                           const uint8_t& n_within,
                           const std::vector<float>& edcs,
                           const uint64_t& future_distance);

  Status AddTrainingSample(const KeyMeta& key_meta, uint64_t cur_seq, uint64_t future_interval);
  // Status AddTrainingExample(const std::vector<double>& features,
  //                           const std::vector<double>& labels);
  void ClearTrainingData();

  Status LogKeyRatio(const ImmutableDBOptions& ioptions);
  Status LogKeyRatioForMultiClass(const ImmutableDBOptions& ioptions, uint64_t num_class);

  Status WriteTrainingData(const std::string& file_path, Env* env  );
  Status WriteTrainingDataForMultiClass(const std::string& file_path, Env* env, uint64_t num_class);

  uint64_t GetNumTrainingSamples() const { return numeric_labels_.size(); } 

private:
  void printConfusionMatrix(const std::vector<int>& y_true, const std::vector<int>& y_pred,
                            int num_classes, std::stringstream& ss);

  Arena* arena_;
  std::vector<float> labels_;
  std::vector<uint64_t> numeric_labels_;
  std::vector<uint64_t> random_access_times_;
  std::vector<int32_t> indptr_;
  std::vector<int32_t> indices_;
  std::vector<double> data_;
  std::vector<double> result_;
  size_t num_features_;
  size_t num_labels_;
  uint64_t batch_size_;
  std::vector<uint64_t> predicted_labes_;
  std::vector<float> weights_;
  std::vector<int32_t> label_count_;

  uint64_t res_short_count_ = 0;
  uint64_t res_long_count_ = 0;


  // const std::vector<float> label_weights_map_ = { 1.0, 4.0, 16.0, 64.0};
  // std::vector<double> result_;


};



}
