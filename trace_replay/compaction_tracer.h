
#pragma once
#include "rocksdb/options.h"
#include "rocksdb/trace_record.h"
#include "rocksdb/trace_reader_writer.h"
#include "monitoring/instrumented_mutex.h"


namespace ROCKSDB_NAMESPACE {
class SystemClock;

struct CompactionTraceRecord {
  uint64_t timestamp;
  std::string drop_key;

  CompactionTraceRecord(uint64_t timestamp, std::string drop_key)
      : timestamp(timestamp), drop_key(drop_key) {}
};

class CompactionTraceWriter {
public:
  virtual ~CompactionTraceWriter() = default;
  virtual Status Write(const CompactionTraceRecord& record) = 0;
};

class CompactionTraceWriterImpl : public CompactionTraceWriter {
public:
  CompactionTraceWriterImpl(SystemClock* clock,
                            std::unique_ptr<TraceWriter>&& trace_writer);

  Status Write(const CompactionTraceRecord& record) override;

private:
  SystemClock* clock_;
  std::unique_ptr<TraceWriter> trace_writer_;
  // InstrumentedMutex mutex_;
};

class CompactionTraceReader {
public:
    CompactionTraceReader(std::unique_ptr<TraceReader>&& trace_reader);
    virtual ~CompactionTraceReader()  ;

  CompactionTraceReader(const CompactionTraceReader&) = delete;
  CompactionTraceReader& operator=(const CompactionTraceReader&) = delete;
  CompactionTraceReader(CompactionTraceReader&&) = delete;
  CompactionTraceReader& operator=(CompactionTraceReader&&) = delete;


  Status Read(CompactionTraceRecord* record);
  private:
    std::unique_ptr<TraceReader> trace_reader_;
};


class CompactionTracer {
public:
  CompactionTracer();
  ~CompactionTracer()  ;


  CompactionTracer(const CompactionTracer&) = delete;
  CompactionTracer& operator=(const CompactionTracer&) = delete;
  CompactionTracer(CompactionTracer&&) = delete;
  CompactionTracer& operator=(CompactionTracer&&) = delete;

  Status StartCompactionTrace(std::unique_ptr<CompactionTraceWriter>&& trace_writer);

  void EndTrace();

  Status WriteDropKey(const CompactionTraceRecord& record);

private:
  InstrumentedMutex mutex_;
  std::atomic<CompactionTraceWriter*> trace_writer_;
};


std::unique_ptr<CompactionTraceWriter> NewCompactionTraceWriter(
  SystemClock* clock, std::unique_ptr<TraceWriter>&& trace_writer );

}
