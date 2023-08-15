
#include <string>

#include "trace_replay/compaction_tracer.h"
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
  trace.type = TraceType::kCompactionDrop;
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
  record->drop_key = drop_key.ToString();
  return Status::OK();

}

std::unique_ptr<CompactionTraceWriter> NewCompactionTraceWriter(SystemClock *clock, 
                                                                std::unique_ptr<TraceWriter> &&trace_writer){
  return std::unique_ptr<CompactionTraceWriter>(new CompactionTraceWriterImpl(clock, std::move(trace_writer)));
}


CompactionHumanReadableTraceWriter::~CompactionHumanReadableTraceWriter() {
  if(human_readable_trace_file_) {
    human_readable_trace_file_->Flush().PermitUncheckedError();
    human_readable_trace_file_->Close().PermitUncheckedError();
  }
}

Status CompactionHumanReadableTraceWriter::NewWritableFile(const std::string &human_readable_trace_file_path, rocksdb::Env *env) {
  if(human_readable_trace_file_path.empty()) {
    return Status::InvalidArgument("human_readable_trace_file_path is empty");
  }

  return env->NewWritableFile(human_readable_trace_file_path, &human_readable_trace_file_, EnvOptions());

}


Status CompactionHumanReadableTraceWriter::WriteHumanReadableTrace(const CompactionTraceRecord &record) {

  if(!human_readable_trace_file_) {
    return Status::InvalidArgument("human_readable_trace_file_ is nullptr");
  }
  int ret = snprintf(trace_record_buffer_, sizeof(trace_record_buffer_), 
                     "%" PRIu64 ",%" PRIu64"\n" , record.timestamp, record.drop_key.size());
  if(ret < 0 || uint64_t(ret) >= sizeof(trace_record_buffer_)) {
    return Status::InvalidArgument("snprintf failed");
  }
  std::string printout(trace_record_buffer_ );
  return human_readable_trace_file_->Append(printout);
}

std::unique_ptr<CompactionTraceReader> NewCompactionTraceReader(std::unique_ptr<TraceReader> &&trace_reader) {
  return std::unique_ptr<CompactionTraceReader>(new CompactionTraceReader(std::move(trace_reader)));
}     


CompactionHumanReadableTraceReader::CompactionHumanReadableTraceReader(const std::string& trace_file_path)
  :CompactionTraceReader(nullptr) {
  human_readable_trace_reader_.open(trace_file_path, std::ifstream::in);
}

CompactionHumanReadableTraceReader::~CompactionHumanReadableTraceReader() {
  human_readable_trace_reader_.close();
}

Status CompactionHumanReadableTraceReader::Read(CompactionTraceRecord* record) {
  std::string line;

  if(!std::getline(human_readable_trace_reader_, line)) {
    return Status::InvalidArgument("getline failed");
  }
  std::stringstream ss(line);
  std::vector<std::string> record_strs;
  while(ss.good()) {
    std::string substr;
    std::getline(ss, substr, ',');
    record_strs.push_back(substr);
  }
  if(record_strs.size() != 2) {
    return Status::InvalidArgument("record_strs.size() != 2");
  }
  record->timestamp = ParseUint64(record_strs[0]);
  uint64_t drop_key_size = ParseUint64(record_strs[1]);
  while(record->drop_key.size() < drop_key_size) {
    record->drop_key += "1";
    }

  return Status::OK();



    

}



CompactionTracer::CompactionTracer()  {trace_writer_.store(nullptr); }

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
