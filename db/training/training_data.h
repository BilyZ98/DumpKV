
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

  void SetShortLabelLimit(uint64_t short_lifetime_threshold) {
    short_label_count_limit_ = short_lifetime_threshold;
  }
  void SetLongLabelLimit(uint64_t long_lifetime_threshold) {
    long_label_count_limit_ = long_lifetime_threshold;
  }
  void SetGCShortLabelLimit(uint64_t short_lifetime_threshold) {
    gc_short_label_limit_ = short_lifetime_threshold;
  }
  void SetGCLongLabelLimit(uint64_t long_lifetime_threshold) {
    gc_long_label_limit_ = long_lifetime_threshold;
  }

  uint64_t GetShortLabelLimit() const {
    return short_label_count_limit_;
  }
  uint64_t GetLongLabelLimit() const {
    return long_label_count_limit_;
  }
  uint64_t GetGCShortLabelLimit() const {
    return gc_short_label_limit_;
  }
  uint64_t GetGCLongLabelLimit() const {
    return gc_long_label_limit_;
  }
  uint64_t GetShortLabelGCCount() const {
    return gc_short_label_count_;
  }
  uint64_t GetLongLabelGCCount() const {
    return gc_long_label_count_;
  }

  uint64_t GetShortLabelCount() const {
    return short_label_count_;
  }
  uint64_t GetLongLabelCount() const {
    return long_label_count_;
  }

  uint64_t GetGCCount() const {
    return gc_sample_count_;
  }
  uint64_t GetCompactionCount() const {
    return compaction_sample_count_;
  }

  Status ConvertLabels(uint64_t threshold);
  Status AddTrainingSample(const std::vector<double>& data, const double& label,
                           const uint64_t& short_lifetime_threshold ) ;
  Status AddGCTrainingSample(const std::vector<double>& data, const double& label,
                             const double& lifetime_idx,
                           const uint64_t& short_lifetime_threshold ) ;

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
  void print_binary_confusion_matrix(const std::vector<int>& y_true, const std::vector<int>& y_pred,
                                   std::stringstream& ss);
  double calculate_precision(int TP, int FP) {
      return static_cast<double>(TP) / (TP + FP);
  }

  double calculate_recall(int TP, int FN) {
      return static_cast<double>(TP) / (TP + FN);
  }

  double calculate_f1_score(double precision, double recall) {
      return 2 * ((precision * recall) / (precision + recall));
  }

  Arena* arena_;
  std::vector<float> labels_;
  std::vector<double> lifetime_idx_;
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

  uint64_t short_label_count_limit_ = 0;
  uint64_t long_label_count_limit_ = 0;
  uint64_t short_label_count_ = 0;
  uint64_t long_label_count_ = 0;

  uint64_t gc_short_label_limit_ = 0;
  uint64_t gc_long_label_limit_ = 0;
  uint64_t gc_short_label_count_ = 0;
  uint64_t gc_long_label_count_ = 0;

  uint64_t gc_sample_count_ = 0;
  uint64_t compaction_sample_count_ = 0;

  uint64_t res_short_count_ = 0;
  uint64_t res_long_count_ = 0;


  // const std::vector<float> label_weights_map_ = { 1.0, 4.0, 16.0, 64.0};
  // std::vector<double> result_;


};



}
