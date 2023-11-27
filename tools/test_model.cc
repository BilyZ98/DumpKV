#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include "file/writable_file_writer.h"
#include "trace_replay/trace_replay.h"
#include "rocksdb/env.h"
#include "rocksdb/utilities/ldb_cmd.h"
#include "LightGBM/c_api.h"

static ROCKSDB_NAMESPACE::Env* FLAGS_env = ROCKSDB_NAMESPACE::Env::Default();
namespace ROCKSDB_NAMESPACE {
class Model {
public:
  struct Trace {
    std::string hexkey;
    std::string key;
    uint64_t timestamp;
    uint64_t key_range;
    uint64_t write_rate_mb_per_sec;
    int label;
    std::vector<double> scores;
  };

  Model(Env* env, EnvOptions env_options, uint64_t classification_num ) 

  :  classification_num_(classification_num),
     env_(env),
    env_options_(env_options) {
    lightgbm_handle_ = nullptr;
    lightgbm_fastConfig_ = nullptr;
    lightgbm_num_iterations_ = 0;
    trace_.scores.resize(classification_num);

  }

  ~Model() {
    LGBM_BoosterFree(lightgbm_handle_);
    LGBM_FastConfigFree(lightgbm_fastConfig_);
  }
  Status LoadKeyRangeFromFile(const std::string &file)  {
    std::ifstream ifs(file);
    if (!ifs.is_open()) {
      return Status::IOError("Failed to open file: " + file);
    }
    std::string key;
    //
    while (std::getline(ifs, key)) {
      // Slice key_slice(key);
      // key_ranges_.push_back(key);
      key_ranges_str_.emplace_back((ROCKSDB_NAMESPACE::LDBCommand::HexToString(std::move(key))));

      key_ranges_.emplace_back(key_ranges_str_.back());
      if(key_ranges_.size() > 1) {
        assert(key_ranges_.back().compare( key_ranges_[key_ranges_.size() - 2]) >=  0);
      }
    }
    return Status::OK();
  }


  // should use humanreadable reader
  // Please refer to blob_cache_tracer.cc 
  // Status BlockCacheHumanReadableTraceReader::ReadAccess(
  // for how to read human readable trace
  Status ReadNextTraceRecord() {

    Status s;
    std::string line;
    if(!std::getline(human_trace_reader_, line)) {
      return Status::Incomplete("No more trace to read");
    }
    std::stringstream ss(line); 
    std::vector<std::string> record_strs;
    while(ss.good()) {
      std::string substr;
      getline(ss, substr, ' ');
      record_strs.push_back(substr);
    }
    const std::string write_symbol = "1";
    if(record_strs.size() == 7 && record_strs[1] == write_symbol) {
      std::string hex_key = record_strs[0];
      uint64_t timestamp = std::stoull(record_strs[4]);
      uint64_t write_rate_mb_per_sec = std::stoull(record_strs[6]);
      trace_.key = ROCKSDB_NAMESPACE::LDBCommand::HexToString(std::move(hex_key));
      trace_.timestamp = timestamp;
      trace_.write_rate_mb_per_sec = write_rate_mb_per_sec;
      trace_.key_range = GetKeyRangeId(trace_.key);
      key_range_map_[trace_.key_range]++;
      trace_.hexkey = hex_key;

      


    } 
  
    // std::string encode_trace;
    // s = trace_reader_->Read(&encode_trace);
    // if(!s.ok()) {
    //   return s;
    // }
    // return TracerHelper::DecodeTrace(encode_trace, trace);
    return s;
  }


  void OutputKeyRange() {
    for(auto& key_range : key_range_map_) {
      printf("key range: %lu, count: %d\n", key_range.first, key_range.second);
    }
  }

  Status OpenTraceFile(std::string& in_path, std::string& output_path) {
    Status s;

    human_trace_reader_.open(in_path, std::ifstream::in);
    
    // s = NewFileTraceReader( env_, env_options_, trace_path, &trace_reader_);
    // if(!s.ok()) {
    //   return s;
    // }

    s = env_->NewWritableFile(output_path, &output_file_, env_options_);
    return s;
  }

  Status StartPrediction() {

    return Status::OK();
  }

  Status EndProcessing() {

    human_trace_reader_.close();
    if(output_file_){
      output_file_->Close();
    }
    return Status::OK();
  }

  Status DoPrediction() {
    std::vector<double> feature_values;
    feature_values.push_back(trace_.timestamp + 10000);
    feature_values.push_back(trace_.write_rate_mb_per_sec);
    feature_values.push_back(trace_.key_range);

    int64_t out_len = 0;
    std::vector<double> out_result(classification_num_, 0.0);
    int predict_res =  LGBM_BoosterPredictForMatSingleRowFast(
        lightgbm_fastConfig_ , feature_values.data(), &out_len, out_result.data());
    if(predict_res != 0) {
      return Status::IOError("Failed to predict");
    }

    for(size_t i=0; i < out_result.size(); i++) {
      trace_.scores[i] = out_result[i];
    }
    int maxIndex = std::distance(out_result.begin(), std::max_element(out_result.begin(), out_result.end()));
    // *label = maxIndex;
    trace_.label = maxIndex;
    return Status::OK();

  }

  // [key, timestamp, key_range, write_rate_mb_per_sec]
  Status WritePredictionResult() {
    int ret = 0;
    ret =snprintf(buffer_, sizeof(buffer_), "%lu %lu %lu %.3f %.3f %d\n",  trace_.timestamp, trace_.key_range, trace_.write_rate_mb_per_sec, trace_.scores[0], trace_.scores[1], trace_.label);
    if(ret < 0) {
      return Status::IOError("Failed to write prediction result");
    }
    std::string printout(buffer_);
    printout = trace_.hexkey +  " " + printout;
    return  output_file_->Append(printout);


  }
  Status LoadModel(const std::string &file_path) {
    // LGBM_BoosterCreateFromModelfile

    int load_res = LGBM_BoosterCreateFromModelfile(file_path.c_str(), &lightgbm_num_iterations_, &lightgbm_handle_);
    if(load_res !=0) {
      return Status::IOError("Failed to load model from file");
    }
    // lightgbm_fastConfig_ = new FastConfigHandle();
    std::string params = "num_threads=1";
    int fast_single_row =  LGBM_BoosterPredictForMatSingleRowFastInit(
        lightgbm_handle_, C_API_PREDICT_NORMAL, 0, 0,
        C_API_DTYPE_FLOAT64, 3, params.c_str(), &lightgbm_fastConfig_
      );
    if(fast_single_row != 0 ) {
      return Status::IOError("Failed to init fast single row");
    }
      return Status::OK();

  }


  int GetKeyRangeId(const Slice& key) const {
    const Comparator*  ucmp = BytewiseComparator();; 
    // const Comparator* const default_cf_ucmp = default_cf->GetComparator();
    // aut
    auto iter = std::lower_bound(key_ranges_.begin(), key_ranges_.end(), key, [ucmp](const Slice & first, const Slice & second){
                                                              return ucmp->Compare(first,second) < 0;
                                                            }) ;
    int dist = std::distance( key_ranges_.begin(), iter);
    return dist;

  }

private:
  std::vector<std::string> key_ranges_str_;
  std::vector<Slice> key_ranges_;
  std::unique_ptr<ROCKSDB_NAMESPACE::WritableFile> output_file_;
  FastConfigHandle lightgbm_fastConfig_;
  BoosterHandle lightgbm_handle_;
  int lightgbm_num_iterations_;
  uint64_t classification_num_;
  // std::unique_ptr<TraceReader> trace_reader_;
  Env *env_;
  EnvOptions env_options_;
  std::string output_path_; 
  std::ifstream human_trace_reader_;
  Trace trace_;
  std::unordered_map<uint64_t, int> key_range_map_;
  
  char buffer_[1024];
};
}


int main(int argc, char** argv) {

  // std::vector<KeyFeatures> key_features;
  std::vector<std::vector<double>> key_features_vec;
  ROCKSDB_NAMESPACE::EnvOptions env_options;
  std::string prefix = "/mnt/nvme0n1/mlsm/test_blob_no_model/with_gc_1.0_0.8/";
  std::string output_path = "model_output.txt";
  std::string key_range_path = prefix + "key_ranges.txt";
  std::string model_path = prefix + "op_keys_binary_lifetime_lightgbm_classification_key_range_model.txt";
  std::string trace_path = prefix + "trace-human_readable_trace.txt"; 
  uint64_t classification_num = 2;

  rocksdb::Status s;
  auto *model = new rocksdb::Model(FLAGS_env, env_options, classification_num);
  s = model->LoadKeyRangeFromFile(key_range_path);
  if(!s.ok()) {
    fprintf(stderr, "Failed to load key range file: %s\n", s.ToString().c_str());
    exit(1);
  }
  s = model->LoadModel(model_path);
  if(!s.ok()) {
    fprintf(stderr, "Failed to load model: %s\n", s.ToString().c_str());
    exit(1);
  }
  s = model->OpenTraceFile(trace_path, output_path); 
  if(!s.ok()) {
    fprintf(stderr, "Failed to open trace file\n");
    assert(false);
  }
  while(s.ok())  {
    s  = model->ReadNextTraceRecord();

    if(!s.ok()) {
      fprintf(stderr, "Failed to read trace record: %s\n", s.ToString().c_str());
      // assert(false);
      break;
    }
    s = model->DoPrediction();
    if(!s.ok()) {
      fprintf(stderr, "Failed to do prediction: %s\n", s.ToString().c_str());
      break;
      // assert(false);
    }

    s = model->WritePredictionResult();
    if(!s.ok()) {
      fprintf(stderr, "Failed to write prediction result: %s\n", s.ToString().c_str());
      break;
      // assert(false);
    }

  }
  model->OutputKeyRange();
  model->EndProcessing();

  delete model;


  return 0;
}
  // prediction code
  // std::vector<double> out_result(classification_num, 0.0);
  // int predict_res =  LGBM_BoosterPredictForMatSingleRowFast(
  //     fast_config_handle_, feature_vec.data(), &out_len, out_result.data());
  // if(predict_res != 0) {
  //   fprintf(stderr, " predict error \n");
  //   assert(false);
  // }

  // int maxIndex = std::distance(out_result.begin(), std::max_element(out_result.begin(), out_result.end()));

  // initiate writefilewriter
  // std::unique_ptr<WritableFileWriter> file_writer;
  // Status s = WritableFileWriter::Create(env->GetFileSystem(), trace_filename,
  //                                       FileOptions(env_options), &file_writer,
  //                                       nullptr);

  // log file format 
  // key, keyrange, prediction, actual , timestamp
  //
  //
  // precision and recall calculation
  // #include <vector>
// #include <iostream>

//   double calculate_precision(const std::vector<double>& predicted, const std::vector<double>& actual) {
//       double tp = 0.0;
//       double fp = 0.0;
//       for (int i = 0; i < predicted.size(); ++i) {
//           if (predicted[i] == 1) {
//               if (actual[i] == 1) {
//                   tp += 1.0;
//               } else {
//                   fp += 1.0;
//               }
//           }
//       }
//       return tp / (tp + fp);
//   }

//   double calculate_recall(const std::vector<double>& predicted, const std::vector<double>& actual) {
//       double tp = 0.0;
//       double fn = 0.0;
//       for (int i = 0; i < predicted.size(); ++i) {
//           if (actual[i] == 1) {
//               if (predicted[i] == 1) {
//                   tp += 1.0;
//               } else {
//                   fn += 1.0;
//               }
//           }
//       }
//       return tp / (tp + fn);
//   }

//   double calculate_f1_score(double precision, double recall) {
//       return 2 * (precision * recall) / (precision + recall);
//   }

//   int main() {
//       std::vector<double> predicted = {1, 0, 1, 1, 0, 1};
//       std::vector<double> actual = {1, 0, 0, 1, 0, 1};

//       double precision = calculate_precision(predicted, actual);
//       double recall = calculate_recall(predicted, actual);
//       double f1_score = calculate_f1_score(precision, recall);

//       std::cout << "Precision: " << precision << std::endl;
//       std::cout << "Recall: " << recall << std::endl;
//       std::cout << "F1 Score: " << f1_score << std::endl;

//       return 0;
//   }



