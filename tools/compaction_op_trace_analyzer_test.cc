

#include <gtest/gtest.h>
#include <functional>
#ifndef GFLAGS
#include <cstdio>
int main() {
  fprintf(stderr, "Please install gflags to run trace_analyzer test\n");
  return 0;
}
#else

#include <functional>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <set>
#include <thread>

#include "db/db_test_util.h"
#include "db/dbformat.h"
#include "file/line_file_reader.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/status.h"
#include "rocksdb/trace_reader_writer.h"
#include "test_util/testharness.h"
#include "test_util/testutil.h"
#include "tools/trace_analyzer_tool.h"
#include "tools/compaction_tracer_analyzer_tool.h"
#include "trace_replay/trace_replay.h"

namespace ROCKSDB_NAMESPACE {
namespace {
static const int kMaxArgCount = 100;
static const size_t kArgBufferSize = 100000;
}  // namespace
class CompactionOpTraceAnalyzerTest : public testing::Test {
public:
  CompactionOpTraceAnalyzerTest() : env_(Env::Default()) {
    // test_path_ = test::PerThreadDBPath("compaction_op_trace_analyzer_test");
    // dbname_ = test_path_ + "/db";
    // env_ = Env::Default();
    // env_->CreateDir(test_path_).PermitUncheckedError();
    GenTestPath();

  }

  void GenTestPath() {
    test_path_ = test::PerThreadDBPath("compaction_op_trace_analyzer_test");
    dbname_ = test_path_ + "/db";
    env_ = Env::Default();
    env_->CreateDir(test_path_).PermitUncheckedError();
  }

  ~CompactionOpTraceAnalyzerTest() override {
    // env_->DeleteDir(test_path_);
  }
  void GenerateTraceWithDelete(std::string op_trace_path, std::string compaction_trace) {
    Options options;
    options.create_if_missing = true;
    WriteOptions wo;
    TraceOptions trace_opt;
    DB* db_ =  nullptr;
    std::string value;
    std::unique_ptr<TraceWriter> trace_writer_;
    std::unique_ptr<TraceWriter> compaction_trace_writer_;

    ASSERT_OK(NewFileTraceWriter(env_, env_options_, op_trace_path, &trace_writer_));
    ASSERT_OK(NewFileTraceWriter(env_, env_options_, compaction_trace, &compaction_trace_writer_));

    ASSERT_OK(DB::Open(options, dbname_, &db_));
    ASSERT_OK(db_->StartTrace(trace_opt, std::move(trace_writer_)));
    ASSERT_OK(db_->StartCompactionTrace(trace_opt, std::move(compaction_trace_writer_)));

    for (int i = 0; i < 100; i++) {
      ASSERT_OK(db_->Put(wo,   std::to_string(i),  std::to_string(i)));
    }


    for (int i = 0; i < 100; i++) {
      ASSERT_OK(db_->Get(ReadOptions(), std::to_string(i), &value));
    }

    for(int i = 0; i < 100; i++) {
      // ASSERT_OK(db_->Put(wo,   std::to_string(i),  std::to_string(i)));
      ASSERT_OK(db_->Delete(wo, std::to_string(i)));
    }

    CompactRangeOptions co;
    ASSERT_OK(db_->CompactRange(co, nullptr, nullptr));


    ASSERT_OK(db_->EndTrace());
    ASSERT_OK(db_->EndCompactionTrace());

    delete db_;
    ASSERT_OK(DestroyDB(dbname_, options));

  }


  void GenerateTrace(std::string op_trace_path, std::string compaction_trace) {
    Options options;
    options.create_if_missing = true;
    WriteOptions wo;
    TraceOptions trace_opt;
    DB* db_ =  nullptr;
    std::string value;
    std::unique_ptr<TraceWriter> trace_writer_;
    std::unique_ptr<TraceWriter> compaction_trace_writer_;

    ASSERT_OK(NewFileTraceWriter(env_, env_options_, op_trace_path, &trace_writer_));
    ASSERT_OK(NewFileTraceWriter(env_, env_options_, compaction_trace, &compaction_trace_writer_));

    ASSERT_OK(DB::Open(options, dbname_, &db_));
    ASSERT_OK(db_->StartTrace(trace_opt, std::move(trace_writer_)));
    ASSERT_OK(db_->StartCompactionTrace(trace_opt, std::move(compaction_trace_writer_)));

    for (int i = 0; i < 100; i++) {
      ASSERT_OK(db_->Put(wo,   std::to_string(i),  std::to_string(i)));
    }


    for (int i = 0; i < 100; i++) {
      ASSERT_OK(db_->Get(ReadOptions(), std::to_string(i), &value));
    }

    for(int i = 0; i < 100; i++) {
      ASSERT_OK(db_->Put(wo,   std::to_string(i),  std::to_string(i)));
    }

    CompactRangeOptions co;
    ASSERT_OK(db_->CompactRange(co, nullptr, nullptr));


    ASSERT_OK(db_->EndTrace());
    ASSERT_OK(db_->EndCompactionTrace());

    delete db_;
    ASSERT_OK(DestroyDB(dbname_, options));

  }

  void RunTraceAnalyzer(const std::vector<std::string>& args, int (func(int, char**))) {
    char arg_buffer[kArgBufferSize];
    char* argv[kMaxArgCount];
    int argc = 0;
    int cursor = 0;

    for (const auto& arg : args) {
      ASSERT_LE(cursor + arg.size() + 1, kArgBufferSize);
      ASSERT_LE(argc + 1, kMaxArgCount);
      snprintf(arg_buffer + cursor, arg.size() + 1, "%s", arg.c_str());

      argv[argc++] = arg_buffer + cursor;
      cursor += static_cast<int>(arg.size()) + 1;
    }

    ASSERT_EQ(0, func(argc, argv) );
  }


  // void AnalyzeCompactionTrace(std::vector<std::string>& paras_diff, std::string output_path, std::string trace_path) {
  //   std::vector<std::string> paras = {"./compaction_trace_analyzer" };
  //   for(auto& para: paras_diff) {
  //     paras.push_back(para);
  //   }

  //   Status s = env_->FileExists(trace_path);
  //   




  // }

  void AnalyzeTrace(std::vector<std::string>& op_paras_diff,
                      std::string op_output_path, std::string op_trace_path,
                    std::vector<std::string>& comp_paras_diff,
                    std::string comp_output_path, std::string comp_trace_path, bool is_delete_trace=false) {
    std::vector<std::string> op_paras = {"./trace_analyzer",
                                       "-convert_to_human_readable_trace",
                                        "-output_key_stats",
                                        "-output_access_count_stats",
                                        "-output_prefix=test",
                                        "-output_prefix_cut=1",
                                        "-output_time_series",
                                        "-output_value_distribution",
                                        "-output_qps_stats",
                                        "-no_print"};

    std::vector<std::string> comp_paras = {"./compaction_trace_analyzer"};


    for(auto& para: op_paras_diff) {
      op_paras.push_back(para);
    }
    for(auto& para: comp_paras_diff) {
      comp_paras.push_back(para);
    }

    Status s = env_->FileExists(op_trace_path);
    if(!s.ok()) {
      if(!is_delete_trace) {
        GenerateTrace(op_trace_path, comp_trace_path);
      }
      else {
        GenerateTraceWithDelete(op_trace_path, comp_trace_path);
      }
    }

    ASSERT_OK(env_->CreateDirIfMissing(op_output_path));

    ASSERT_OK(env_->CreateDirIfMissing(comp_output_path));
    RunTraceAnalyzer(op_paras, &ROCKSDB_NAMESPACE::trace_analyzer_tool);
    RunTraceAnalyzer(comp_paras, &ROCKSDB_NAMESPACE::compaction_tracer_analyzer_main);

  }

  void GetFileContent(std::string file_path, std::vector<std::string> &results) {
    const auto& fs = env_->GetFileSystem();
    FileOptions fopts(env_options_);

    ASSERT_OK(fs->FileExists(file_path, fopts.io_options, nullptr));
    std::unique_ptr<FSSequentialFile> file;
    ASSERT_OK(fs->NewSequentialFile(file_path, fopts, &file, nullptr));

    LineFileReader lf_reader(std::move(file), file_path,
                             4096 /* filereadahead_size */);

    // std::vector<std::string> result;
    std::string line;
    while (
        lf_reader.ReadLine(&line, Env::IO_TOTAL /* rate_limiter_priority */)) {
        results.push_back(line);
    }

    ASSERT_OK(lf_reader.GetStatus());


  }
  void SplitString(const std::string& s, char delim, std::vector<std::string>* result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
      result->push_back(item);
    }
  }
  Status ExtractOpKeyandSequence(std::vector<std::string>& human_trace, std::unordered_map<std::string, std::set<SequenceNumber>>& results) {
    for(auto& line: human_trace) {
      std::vector<std::string> line_result;
      SplitString(line, ' ', &line_result);
      std::string op_type= line_result[1];
      if(!(op_type == "1" or op_type == "2"))  {
        continue;

      }

      if(line_result.size() != 6) {
        return Status::Corruption("bad human trace");
      }
      std::string key = line_result[0];
      SequenceNumber seq = std::stoul(line_result[5]);
      results[key].insert(seq);
    }
    return Status::OK();
  }


  Status ExtractCompactionKeyAndSequence(std::vector<std::string>& results, std::vector<std::string>& keys, std::vector<SequenceNumber>& seqs) {
    Status s;
    for(auto& line: results) {

      std::vector<std::string> line_result;
      SplitString(line, ' ', &line_result);
      if(line_result.size() != 3) {
        return Status::Corruption("bad human trace");
      }

      // Slice key = line.substr(0, line.find(' '));
      // ParsedInternalKey ikey;
      // s = ParseInternalKey(key, &ikey, true);
      // if (!s.ok()) {
      //   return Status::Corruption("bad internal key");
      // }

      SequenceNumber seq = std::stoul(line_result[1]);
      keys.push_back(line_result[0]);
      seqs.push_back(seq);
    }
    return Status::OK();
  }

  std::string test_path_;
  std::string dbname_;
  EnvOptions env_options_;
  ROCKSDB_NAMESPACE::Env* env_;

};


TEST_F(CompactionOpTraceAnalyzerTest, Test1) {
  std::string op_trace_path = test_path_ + "/trace";
  std::string output_path = test_path_ + "/output";
  std::string comp_trace_path = test_path_ + "/compaction_trace";
  std::vector<std::string> op_paras_diff = {"-analyze_put=true",
                                       "-trace_path=" + op_trace_path,
                                         "-output_dir=" + output_path};

  std::vector<std::string> comp_paras_diff = {"-compaction_trace_path="+comp_trace_path,
                                            "-compaction_output_dir="+output_path};

  AnalyzeTrace(op_paras_diff, output_path, op_trace_path, comp_paras_diff,  output_path, comp_trace_path, false);
  std::vector<std::string> op_results;
  std::vector<std::string> comp_results;
  std::string op_human_file_path = output_path + "/test-human_readable_trace.txt";
  GetFileContent(op_human_file_path, op_results);
  std::string comp_human_file_path = output_path + "/compaction_human_readable_trace.txt";
  GetFileContent(comp_human_file_path, comp_results);
  
  std::vector<std::string> comp_userkey;
  std::vector<SequenceNumber> comp_seqs;
  ExtractCompactionKeyAndSequence(comp_results, comp_userkey, comp_seqs);

  std::unordered_map<std::string, std::set<SequenceNumber>> op_userkey;
  ExtractOpKeyandSequence(op_results, op_userkey);

  for(size_t i = 0; i < comp_userkey.size(); i++) {
    std::string key = comp_userkey[i];
    SequenceNumber seq = comp_seqs[i];
    
    ASSERT_TRUE(op_userkey.find(key) != op_userkey.end());
    ASSERT_TRUE(op_userkey[key].find(seq) != op_userkey[key].end()); 
    EXPECT_TRUE(op_userkey[key].size() == 2);
    // ASSERT_TRUE(op_userkey[key].size() == 2);

  }


}



TEST_F(CompactionOpTraceAnalyzerTest, TestDelete) {
  
  GenTestPath();
  std::string op_trace_path = test_path_ + "/trace";
  std::string output_path = test_path_ + "/output";
  std::string comp_trace_path = test_path_ + "/compaction_trace";
  std::vector<std::string> op_paras_diff = {"-analyze_put=true",
                                        "-analyze_delete=true",
                                       "-trace_path=" + op_trace_path,
                                         "-output_dir=" + output_path};

  std::vector<std::string> comp_paras_diff = {"-compaction_trace_path="+comp_trace_path,
                                            "-compaction_output_dir="+output_path};

  AnalyzeTrace(op_paras_diff, output_path, op_trace_path, comp_paras_diff,  output_path, comp_trace_path, true);
  std::vector<std::string> op_results;
  std::vector<std::string> comp_results;
  std::string op_human_file_path = output_path + "/test-human_readable_trace.txt";
  GetFileContent(op_human_file_path, op_results);
  std::string comp_human_file_path = output_path + "/compaction_human_readable_trace.txt";
  GetFileContent(comp_human_file_path, comp_results);
  
  std::vector<std::string> comp_userkey;
  std::vector<SequenceNumber> comp_seqs;
  ExtractCompactionKeyAndSequence(comp_results, comp_userkey, comp_seqs);

  std::unordered_map<std::string, std::set<SequenceNumber>> op_userkey;
  ExtractOpKeyandSequence(op_results, op_userkey);

  for(size_t i = 0; i < comp_userkey.size(); i++) {
    std::string key = comp_userkey[i];
    SequenceNumber seq = comp_seqs[i];
    
    ASSERT_TRUE(op_userkey.find(key) != op_userkey.end());
    ASSERT_TRUE(op_userkey[key].find(seq) != op_userkey[key].end()); 
    EXPECT_TRUE(op_userkey[key].size() == 2);
    // ASSERT_TRUE(op_userkey[key].size() == 2);

  }

}

}

int main(int argc, char** argv) {
ROCKSDB_NAMESPACE::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}




#endif
