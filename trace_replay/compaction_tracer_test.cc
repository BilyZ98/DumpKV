#include "trace_replay/compaction_tracer.h"

#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/trace_reader_writer.h"
#include "rocksdb/trace_record.h"

#include "test_util/testutil.h"
#include "test_util/testharness.h"
namespace ROCKSDB_NAMESPACE {

namespace {
const std::string kKeyPrefix = "test-compaction-";
}
class CompactionTracerTest: public testing::Test {
public:
  CompactionTracerTest() {
    test_path_ = test::PerThreadDBPath( "compaction_tracer_test");
    env_ =  ROCKSDB_NAMESPACE::Env::Default();
    clock_ = env_->GetSystemClock().get();
    EXPECT_OK(env_->CreateDir(test_path_));
    trace_file_path_ = test_path_ + "/trace_file";

  }

  ~CompactionTracerTest() override {
    EXPECT_OK( env_->DeleteFile(trace_file_path_));
    EXPECT_OK( env_->DeleteDir(test_path_));

  }


  void WriteDropKeys(CompactionTraceWriter* writer, uint32_t from_key_id,
                     uint32_t nkeys) {

    assert(writer != nullptr);
    for(uint32_t i=0; i < nkeys; i++) {
      CompactionTraceRecord record(clock_->NowMicros(), kKeyPrefix + std::to_string(from_key_id + i));

      ASSERT_OK(writer->Write(record));
    }

  }


  CompactionTraceRecord GenerateDropKeyRecord() {
    return CompactionTraceRecord(clock_->NowMicros(), kKeyPrefix + std::to_string(0));
  }

  Env* env_;
  SystemClock* clock_;
  std::string test_path_;
  EnvOptions env_options_;
  std::string trace_file_path_;


};

TEST_F(CompactionTracerTest, WriteBeforeStart) {
  CompactionTraceRecord record = GenerateDropKeyRecord();
  {
    std::unique_ptr<TraceWriter> trace_writer;
    ASSERT_OK(NewFileTraceWriter(env_, env_options_, trace_file_path_, &trace_writer ));

    CompactionTracer tracer;
    ASSERT_OK(tracer.WriteDropKey(record));
    ASSERT_OK(env_->FileExists(trace_file_path_));
  }

  {

  }

}



}
int main(int argc, char** argv) {
  ROCKSDB_NAMESPACE::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
