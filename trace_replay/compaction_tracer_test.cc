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


  void VerifyAccess(CompactionTraceReader* reader, uint32_t from_key_id,
                    uint32_t nkeys) {
    assert(reader != nullptr);
    for(uint32_t i=0; i < nkeys; i++) {
      CompactionTraceRecord record;
      ASSERT_OK(reader->Read(&record));
      ASSERT_EQ(record.drop_key, kKeyPrefix + std::to_string(from_key_id + i));
    }
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

TEST_F(CompactionTracerTest, AtomicWrite) {
  CompactionTraceRecord record = GenerateDropKeyRecord();
  {
    std::unique_ptr<TraceWriter> trace_writer;
    ASSERT_OK(NewFileTraceWriter(env_, env_options_, trace_file_path_, &trace_writer ));

    std::unique_ptr<CompactionTraceWriter> compaction_trace_writer(new CompactionTraceWriterImpl(clock_, std::move(trace_writer)));
    ASSERT_NE(compaction_trace_writer, nullptr);

    CompactionTracer tracer;
    ASSERT_OK(tracer.StartCompactionTrace(std::move(compaction_trace_writer)));
    ASSERT_OK(tracer.WriteDropKey(record));
    ASSERT_OK(env_->FileExists(trace_file_path_));
  }

  {
    std::unique_ptr<TraceReader> trace_reader;
    ASSERT_OK(NewFileTraceReader(env_, env_options_, trace_file_path_, &trace_reader ));
    CompactionTraceReader compaction_trace_reader(std::move(trace_reader));
    CompactionTraceRecord read_record;
    VerifyAccess(&compaction_trace_reader, 0, 1);
    ASSERT_NOK(compaction_trace_reader.Read(&read_record));
    // ASSERT_OK(trace_reader->Read(&read_record));
    // ASSERT_EQ(read_record.drop_key, record.drop_key);
  }
}


TEST_F(CompactionTracerTest, ConsecutiveStartTrace) {
  std::unique_ptr<TraceWriter> trace_writer;
  ASSERT_OK(NewFileTraceWriter(env_, env_options_, trace_file_path_, &trace_writer ));
  std::unique_ptr<CompactionTraceWriter> compaction_trace_writer(new CompactionTraceWriterImpl(clock_, std::move(trace_writer)));
  ASSERT_NE(compaction_trace_writer, nullptr);
  
  CompactionTracer tracer;
  ASSERT_OK(tracer.StartCompactionTrace(std::move(compaction_trace_writer)));
  ASSERT_NOK(tracer.StartCompactionTrace(std::move(compaction_trace_writer)));

  ASSERT_OK(env_->FileExists(trace_file_path_));



}






}
int main(int argc, char** argv) {
  ROCKSDB_NAMESPACE::port::InstallStackTraceHandler();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


















