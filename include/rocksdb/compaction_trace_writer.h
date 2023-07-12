#pragma once
#include "rocksdb/trace_record.h"
#include "rocksdb/system_clock.h"


#include "rocksdb/trace_reader_writer.h"
namespace ROCKSDB_NAMESPACE {
struct CompactionTraceRecord {
  uint64_t timestamp = 0;
  std::string drop_key;

  CompactionTraceRecord() = default;
  CompactionTraceRecord(uint64_t _timestamp, std::string _drop_key)
      : timestamp(_timestamp), drop_key(_drop_key) {}
};

class CompactionTraceWriter {
public:
  virtual ~CompactionTraceWriter() = default;
  virtual Status Write(const CompactionTraceRecord& record) = 0;
};

std::unique_ptr<CompactionTraceWriter> NewCompactionTraceWriter(
  SystemClock* clock, std::unique_ptr<TraceWriter>&& trace_writer );

}
