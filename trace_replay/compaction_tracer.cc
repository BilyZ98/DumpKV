#include "trace_replay/compaction_tracer.h"

#include <string>

#include "db/db_impl/db_impl.h"
#include "db/dbformat.h"
#include "rocksdb/slice.h"
#include "rocksdb/trace_record.h"
#include "util/coding.h"
#include "util/hash.h"
#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {

CompactionTraceWriterImpl::CompactionTraceWriterImpl(
    SystemClock* clock, std::unique_ptr<TraceWriter>&& trace_writer)
    : clock_(clock), trace_writer_(std::move(trace_writer)) {}

Status CompactionTraceWriterImpl::Write(const CompactionTraceRecord& record) {
  uint64_t trace_file_size = trace_writer_->GetFileSize(); 
  uint64_t max_trace_file_size = uint64_t{64}* 1024 * 1024 * 1024; 
  if(trace_file_size > max_trace_file_size) {
    return Status::OK();
  }
  Trace trace;
  trace.ts = record.timestamp;
  PutLengthPrefixedSlice(&trace.payload, record.drop_key);
  std::string encoded_trace;
  TracerHelper::EncodeTrace(trace, &encoded_trace);
  return trace_writer_->Write(encoded_trace);

}

CompactionTraceReader::CompactionTraceReader(
    std::unique_ptr<TraceReader>&& trace_reader)
    : trace_reader_(std::move(trace_reader)) {}


CompactionTraceReader::~CompactionTraceReader() {
  trace_reader_->Close();
}

Status CompactionTraceReader::Read(CompactionTraceRecord* record) {
  assert(record != nullptr);
  std::string encoded_trace;
  Status s = trace_reader_->Read(&encoded_trace);
  if (!s.ok()) {
    return s;
  }
  Trace trace;
  s = TracerHelper::DecodeTrace(encoded_trace, &trace);
  if(!s.ok()) {
    return s;
  }
  record->timestamp = trace.ts;
  Slice enc_slice = Slice(trace.payload);
  Slice drop_key;
  if(!GetLengthPrefixedSlice(&enc_slice, &drop_key)) {
    return Status::Incomplete("drop_key");
  }
  return Status::OK();

}

std::unique_ptr<CompactionTraceWriter> NewCompactionTraceWriter(SystemClock *clock, 
                                                                std::unique_ptr<TraceWriter> &&trace_writer){
  return std::unique_ptr<CompactionTraceWriter>(new CompactionTraceWriterImpl(clock, std::move(trace_writer)));
}



CompactionTracer::CompactionTracer() : trace_writer_(nullptr) {}

CompactionTracer::~CompactionTracer() {
  EndTrace(); 
}

Status CompactionTracer::StartCompactionTrace(std::unique_ptr<CompactionTraceWriter> &&trace_writer) {
  if (trace_writer_.load()) {
    return Status::Busy();
  }
  
  // trace_writer_ = std::move(trace_writer);
  trace_writer_.store(trace_writer.release());
  return Status::OK();
}

void CompactionTracer::EndTrace() {
  InstrumentedMutexLock l(&mutex_);
  if (!trace_writer_.load()) {
    return;
  }
  delete trace_writer_.load();
  trace_writer_.store(nullptr);
}

Status CompactionTracer::WriteDropKey(const CompactionTraceRecord &record) {
  if (!trace_writer_.load()) {
    return Status::OK();
  }
  InstrumentedMutexLock l(&mutex_);
  if(!trace_writer_.load()) {
    return Status::OK();
  }
  return trace_writer_.load()->Write(record);
}
}
