
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
#include <utility>
#include <vector>


#include "rocksdb/key_meta.h"
#include "rocksdb/status.h"
#include "memory/allocator.h"
#include "memory/concurrent_arena.h"
#include "LightGBM/c_api.h"

namespace ROCKSDB_NAMESPACE {

class Arena;
class TrainingData{
public:
  TrainingData(Arena* arena, size_t num_features, 
               uint64_t batch_size,
               size_t num_labels=2);
  Status TrainModel(BoosterHandle* new_model,  const std::unordered_map<std::string, std::string>& training_params ); 
  // Status AddTrainingSample()

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

  Status WriteTrainingData(const std::string& file_path, Env* env);

  uint64_t GetNumTrainingSamples() const { return labels_.size(); } 

private:
  Arena* arena_;
  std::vector<float> labels_;
  std::vector<int32_t> indptr_;
  std::vector<int32_t> indices_;
  std::vector<double> data_;
  std::vector<double> result_;
  size_t num_features_;
  size_t num_labels_;
  uint64_t batch_size_;
  uint64_t res_short_count_ = 0;
  uint64_t res_long_count_ = 0;


  // std::vector<double> result_;


};



}
