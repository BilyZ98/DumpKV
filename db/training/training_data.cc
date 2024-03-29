
#include <cmath>
#include "db/training/training_data.h"
#include <cstdint>
#include "rocksdb/key_meta.h"
#include "rocksdb/status.h"
#include "LightGBM/c_api.h"
#include "options/db_options.h"
#include "logging/logging.h"



namespace ROCKSDB_NAMESPACE {


TrainingData::TrainingData(Arena* arena, 
                           size_t num_features, 
                          uint64_t batch_size,
                           size_t num_labels)
    : arena_(arena),
      num_features_(num_features),
      num_labels_(num_labels),
      batch_size_(batch_size){

  labels_.reserve(batch_size_);
  indptr_.reserve(batch_size_ + 1);
  indptr_.emplace_back(0);
  indices_.reserve(batch_size_ * (num_features + 1));
  data_.reserve(batch_size_ * (num_features + 1));
  weights_.reserve(batch_size_);
  label_count_.resize(LifetimeSequence.size(), 0);
}

Status TrainingData::AddTrainingSample(const std::vector<double>& data, const double& label ) {

  int32_t counter = indptr_.back();
  for(size_t i = 0; i < data.size(); ++i) {
    indices_.emplace_back(i);
    data_.emplace_back(data[i]);
    counter++;
  }
  assert(data.size() <= num_features_);
  uint64_t label_uint64_t = static_cast<uint64_t>(label);
  const auto& lifetime_index_label_iter = std::lower_bound(LifetimeSequence.begin(), LifetimeSequence.end(), label_uint64_t) ;
  uint32_t lifetime_index = std::distance(LifetimeSequence.begin(), lifetime_index_label_iter); 
  if(lifetime_index == LifetimeSequence.size()) {
    lifetime_index = LifetimeSequence.size() - 1;
  }
  // labels_.push_back(log1p(label));
  labels_.push_back(lifetime_index);
  label_count_[lifetime_index]++;
  indptr_.push_back(counter);
  return Status::OK();
}

Status TrainingData::AddTrainingSample(const std::vector<uint64_t>& past_distances,
                         const uint64_t& blob_size,
                         const uint8_t& n_within,
                         const std::vector<float>& edcs,
                         const uint64_t& future_distance) {
  int32_t counter = indptr_.back();
  assert(edcs.size() > 0);
  assert(!past_distances.empty());
  assert(past_distances.size() <= max_n_past_timestamps);
  size_t j = 0;
  // max_n_past_timestamps or max_n_past_distances ?
  for(; j < past_distances.size() && j < max_n_past_timestamps; ++j) {
    indices_.emplace_back(j);
    data_.emplace_back(static_cast<double>(past_distances[j]));
    counter++;
  }
  indices_.emplace_back(max_n_past_timestamps);
  data_.push_back(static_cast<double>(blob_size));
  ++counter;

  indices_.emplace_back(max_n_past_timestamps+1);
  data_.emplace_back(n_within);
  ++counter;
  for (int k = 0; k < n_edc_feature; ++k) {
    indices_.push_back(max_n_past_timestamps  + 2 + k);
    data_.push_back(static_cast<double>(edcs[k]));
  }
  counter += n_edc_feature;

  labels_.push_back(log1p(future_distance));
  // const uint64_t seq_delta_threshold = 500000; 
  // if(future_distance > seq_delta_threshold) {
  //   labels_.push_back(1.0);
  // } else {
  //   labels_.push_back(0.0);
  // }
  indptr_.push_back(counter);


  return Status::OK();

}
Status TrainingData::AddTrainingSample(const KeyMeta& key_meta, uint64_t cur_seq, uint64_t future_interval) {

  int32_t counter = indptr_.back();
  const auto& meta_extra = key_meta._extra;
  //
  // indices_.emplace_back(0);
  // data_.emplace_back(cur_seq- key_meta.past_sequence_);
  // counter++;


  uint32_t this_past_distance = 0;
  int j = 0;
  uint8_t n_within = 0;
  if (key_meta._extra) {
    for (; j < key_meta._extra->_past_distance_idx && j < max_n_past_distances; ++j) {
        uint8_t past_distance_idx = (key_meta._extra->_past_distance_idx - 1 - j) % max_n_past_distances;
        const uint32_t &past_distance = key_meta._extra->_past_distances[past_distance_idx];
        this_past_distance += past_distance;
        indices_.emplace_back(j  );
        data_.emplace_back(past_distance);
        if (this_past_distance < memory_window) {
            ++n_within;
        }
    }
  }

  counter += j;

  indices_.emplace_back(max_n_past_timestamps);
  data_.push_back(key_meta.size_);
  ++counter;

  indices_.emplace_back(max_n_past_timestamps+1);
  data_.emplace_back(n_within);
  ++counter;

  // for (const auto& edc : meta_extra->_edc) {
  // for(size_t k = 0; k < n_edc_feature; ++k) {
  //   indices_.emplace_back(max_n_past_timestamps+k+2);
  //   data_.emplace_back(meta_extra->_edc[k]);
  // }
  // indptr_.emplace_back(counter);

  if(meta_extra != nullptr) {
    for (int k = 0; k < n_edc_feature; ++k) {
        indices_.push_back(max_n_past_timestamps  + 2 + k);
        // uint32_t _distance_idx = std::min(
        //         uint32_t(cur_seq - key_meta.past_sequence_) / edc_windows[k],
        //         max_hash_edc_idx);
        // data_.push_back(meta_extra->_edc[k] * hash_edc[_distance_idx]);
      data_.push_back(meta_extra->_edc[k]);
    }
  } else {
   for (int k = 0; k < n_edc_feature; ++k) {
      indices_.push_back(max_n_past_timestamps +  2 + k);
      uint32_t _distance_idx = std::min(
              uint32_t(cur_seq - key_meta.past_sequence_) / edc_windows[k],
              max_hash_edc_idx);
      // data_.push_back(hash_edc[_distance_idx]);
      data_.push_back(1);
    }
  }
  counter += n_edc_feature;

  // labels_.push_back(log1p(future_interval));
  const uint64_t seq_delta_threshold = 1000000; 
  // if(cur_seq - key_meta.past_sequence_ > seq_delta_threshold) {
  if(future_interval > seq_delta_threshold) {
    labels_.push_back(1.0);
  } else {
    labels_.push_back(0.0);
  }
  indptr_.push_back(counter);


  return Status::OK();

}




Status TrainingData::TrainModel(BoosterHandle* new_model_ptr,  const std::unordered_map<std::string, std::string>& training_params )  {
  // Convert unordered_map to key=value format
  std::string params_string;
  for (const auto& kv : training_params) {
    params_string += kv.first + "=" + kv.second + " ";
      // converted_params.push_back(kv.first + "=" + kv.second);
  }
  uint64_t num_iterations = std::stoi(training_params.at("num_iterations")) - 1;
  auto params = params_string.c_str();
  int res = 0;


  // Prepare parameters for LGBM_DatasetCreateFromCSR
  // if (booster) LGBM_BoosterFree(booster);
  // create training dataset
  DatasetHandle trainData;
  res = LGBM_DatasetCreateFromCSR(
          static_cast<void *>(indptr_.data()),
          C_API_DTYPE_INT32,
          indices_.data(),
          static_cast<void *>(data_.data()),
          C_API_DTYPE_FLOAT64,
          indptr_.size(),
          data_.size(),
          num_features_,  //remove future t
          params,
          nullptr,
          &trainData);
  assert(res == 0 );

  res = LGBM_DatasetSetField(trainData,
                       "label",
                       static_cast<void *>(labels_.data()),
                       labels_.size(),
                       C_API_DTYPE_FLOAT32);
  assert(res == 0);
  
  uint64_t sample_size = labels_.size();
  std::vector<float> label_weights(LifetimeSequence.size(), 0);
  for(size_t i = 0; i < label_count_.size(); ++i) {
    if(label_count_[i] == 0) {
      continue;
    }

    label_weights[i] = float(sample_size) /  label_count_[i] ;
  }
  for(size_t i = 0; i < labels_.size(); ++i) {
    weights_.emplace_back(label_weights[labels_[i]]);
  }

  res = LGBM_DatasetSetField(trainData,
                       "weight",
                       static_cast<void *>(weights_.data()),
                       weights_.size(),
                       C_API_DTYPE_FLOAT32);

  assert(res == 0);

  // init booster
  res = LGBM_BoosterCreate(trainData, params, new_model_ptr);
  if (res != 0) {
    assert(false);
  }

  // train
  for (int i = 0; i < stoi(training_params.at("num_iterations")); i++) {
    int isFinished;
    res = LGBM_BoosterUpdateOneIter(*new_model_ptr, &isFinished);
    if(res != 0) {
      assert(false);
      return Status::Incomplete("LGBM_BoosterUpdateOneIter failed");
    }
    if (isFinished) {
        break;
    }
  }

  size_t num_class = stoi(training_params.at("num_class"));
  int64_t len;
  // std::vector<double> result(indptr_.size() - 1);
  uint64_t result_size = (indptr_.size()-1) * num_class;
  predicted_labes_.resize(indptr_.size() - 1);
  // result_.resize(indptr_.size() - 1);
  result_.resize(result_size);
  res = LGBM_BoosterPredictForCSR(*new_model_ptr,
                            static_cast<void *>(indptr_.data()),
                            C_API_DTYPE_INT32,
                            indices_.data(),
                            static_cast<void *>(data_.data()),
                            C_API_DTYPE_FLOAT64,
                            indptr_.size(),
                            data_.size(),
                            num_features_,  //remove future t
                            C_API_PREDICT_NORMAL,
                            0,
                            num_iterations,
                            params,
                            &len,
                            result_.data());

  assert(res == 0);
  if(num_class > 2) {
    size_t label_idx = 0;
    for(size_t i = 0; i < result_.size(); i+=num_class, label_idx++) {
      int32_t max_idx=0;
      for(size_t k = 0; k < num_class; ++k) {
        if(result_[i+k] > result_[i+max_idx]) {
          max_idx = k;
        }
      }
      predicted_labes_[label_idx] = max_idx;
      if(predicted_labes_[label_idx] == labels_[label_idx]) {
        res_long_count_++;
      } else {
        res_short_count_++;
      }

    }

  }
  // for(size_t i = 0; i < result.size(); ++i) {
  //   if(result[i] > 0.5) {
  //     res_long_count_++;
  //   } else {
  //     res_short_count_++;
  //   }
  //   // fprintf(stdout, "%f\n", result[i]);
  //   // std::cout << result[i] << std::endl;
  // }
  // fprintf(stdout, "short_count: %lu, long_count: %lu\n", res_short_count_, res_long_count_);

  if(res != 0) {
    assert(false);
    return Status::Incomplete("LGBM_BoosterPredictForCSR failed");
  }

  res = LGBM_DatasetFree(trainData);
  if(res != 0) {
    assert(false);
    return Status::Incomplete("LGBM_DatasetFree failed");
  }
  // *new_model_ptr = new_model;
  return Status::OK();

}

void TrainingData::ClearTrainingData() {
  labels_.clear();
  indptr_.clear();
  indices_.clear();
  data_.clear();  
  result_.clear();
  indptr_.emplace_back(0);
  weights_.clear();
  label_count_.resize(LifetimeSequence.size(), 0);
  res_short_count_ = 0;
  res_long_count_ = 0;
}

Status TrainingData::WriteTrainingDataForMultiClass(const std::string& file_path, Env* env, uint64_t num_class) {

  std::unique_ptr<WritableFile> file;
  const EnvOptions soptions;
  Status s = env->NewWritableFile(file_path, &file, soptions);
  if (!s.ok()) {
    return s;
  }
  // write csr format data to file
  for (size_t i = 0; i < indptr_.size()-1; ++i) {
    uint64_t cur_pos = indptr_[i];
    uint64_t next_pos = indptr_[i+1];
    for (uint64_t j = cur_pos; j < next_pos; ++j) {
      

      std::string data_with_sep =  std::to_string(data_[j]) + " ";
      s = file->Append(data_with_sep);
      if (!s.ok()) {
        return s;
      }
    }

    for(size_t k = 0; k < num_class; ++k) {
      size_t result_idx = i*num_class + k;
      std::string predict_result_str =   std::to_string(result_[result_idx])  + " ";
      s = file->Append(predict_result_str);
      if (!s.ok()) {
        return s;
      }
    }

    std::string label_str = std::to_string(labels_[i]) + " " + std::to_string(predicted_labes_[i]) + "\n";
    s = file->Append(label_str);
    if (!s.ok()) {
      return s;
    }

  }
  file->Close();
  return Status::OK();


}
Status TrainingData::WriteTrainingData(const std::string& file_path, Env* env) {
  std::unique_ptr<WritableFile> file;
  const EnvOptions soptions;
  Status s = env->NewWritableFile(file_path, &file, soptions);
  if (!s.ok()) {
    return s;
  }
  // write csr format data to file
  for (size_t i = 0; i < indptr_.size()-1; ++i) {
    uint64_t cur_pos = indptr_[i];
    uint64_t next_pos = indptr_[i+1];
    for (uint64_t j = cur_pos; j < next_pos; ++j) {
      

      std::string data_with_sep =  std::to_string(data_[j]) + " ";
      s = file->Append(data_with_sep);
      if (!s.ok()) {
        return s;
      }
    }

    std::string label_str = std::to_string(labels_[i]) + " " + std::to_string(result_[i]) + "\n";
    s = file->Append(label_str);
    if (!s.ok()) {
      return s;
    }

  }
  file->Close();
  return Status::OK();


}
void TrainingData::printConfusionMatrix(const std::vector<int>& y_true, const std::vector<int>& y_pred, 
                                        int num_class, std::stringstream& ss) {
    std::map<std::pair<int, int>, int> confusionMatrix;

    for (size_t i = 0; i < y_true.size(); i++) {
        confusionMatrix[std::make_pair(y_true[i], y_pred[i])]++;
    }

    // std::cout << "Confusion Matrix: \n";
    for (int i = 0; i < num_class; i++) {
        for (int j = 0; j < num_class; j++) {
            ss << confusionMatrix[std::make_pair(i, j)] << "\t";
        }
        ss << "\n";
    }
}


Status TrainingData::LogKeyRatioForMultiClass(const ImmutableDBOptions& ioptions, uint64_t num_class) {
  std::vector<int> label_count(num_class, 0);
  for(const auto &label: labels_) {
    int label_int = static_cast<int>(label);
    label_count[label_int]++;
  }
  std::string result_str;
  for(size_t i = 0; i < label_count.size(); ++i) {
    result_str += std::to_string(i) + ":" + std::to_string(label_count[i]) + " ";
  }

  std::vector<int> predicted_label_count(num_class, 0);
  for(const auto &label: predicted_labes_) {
    int label_int = static_cast<int>(label);
    predicted_label_count[label_int]++;
  }
  std::string predicted_result_str;
  for(size_t i = 0; i < predicted_label_count.size(); ++i) {
    predicted_result_str += std::to_string(i) + ":" + std::to_string(predicted_label_count[i]) + " ";
  }

  std::string confusion_matrix_str;
  std::stringstream ss;
  std::vector<int> labels_int(labels_.size());
  for(size_t i = 0; i < labels_.size(); ++i) {
    labels_int[i] = static_cast<int>(labels_[i]);
  }
  std::vector<int> predicted_labes_int(predicted_labes_.size());
  for(size_t i = 0; i < predicted_labes_.size(); ++i) {
    predicted_labes_int[i] = static_cast<int>(predicted_labes_[i]);
  }

  printConfusionMatrix(labels_int, predicted_labes_int, num_class, ss);

  ROCKS_LOG_INFO(
          ioptions.info_log,
          "Train data label %s"
          " res_short_count: %lu, res_long_count: %lu"
          " predicted label %s",
          result_str.c_str(), res_short_count_, res_long_count_,
          predicted_result_str.c_str());

  ROCKS_LOG_INFO(ioptions.info_log, "Confusion Matrix: \n%s", ss.str().c_str());
  
   return Status::OK();

}
Status TrainingData::LogKeyRatio(const ImmutableDBOptions& ioptions) {
  uint64_t short_count = 0;  
  uint64_t long_count = 0 ;
  for(const auto &label: labels_) {
    if(label > 0.9) {
      long_count++;
    } else {
      short_count++;
    }

  }

  ROCKS_LOG_INFO(
          ioptions.info_log,
          "key label ratio 0: %lu, 1: %lu"
          " res_short_count: %lu, res_long_count: %lu",
          short_count, long_count, res_short_count_, res_long_count_);
  return Status::OK();

}
}
