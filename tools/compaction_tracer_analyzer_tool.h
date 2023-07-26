#pragma once

#include <string>
#include "rocksdb/env.h"
#include "rocksdb/trace_reader_writer.h"
#include "rocksdb/compaction_trace_writer.h"
#include "trace_replay/compaction_tracer.h"
#include "rocksdb/trace_record.h"

namespace ROCKSDB_NAMESPACE {
class CompactionTraceAnalyzerTool  {
 public:
  CompactionTraceAnalyzerTool(std::string& trace_path, std::string & output_path ) ; 
  ~CompactionTraceAnalyzerTool() {}


  Status PreProcessing();
  Status StartPorcessing();
 private:

  Status OutputAnalysisResult(const std::string& output_path,
                              const std::string& result);
  // void AnalyzeCompactionTrace(const std::string& trace_path,
  //                             const std::string& output_path);
  ROCKSDB_NAMESPACE::Env* env_;
  EnvOptions env_options_;
  std::unique_ptr<TraceReader> trace_reader_;
  std::string trace_path_;
  std::string output_path_;
  char buffer[1024];
  std::unique_ptr<CompactionTraceReader> compaction_trace_reader_;
  std::unique_ptr<ROCKSDB_NAMESPACE::WritableFile> trace_sequence_f_;
};


int compaction_tracer_analyzer_main(int argc, char** argv); 
}
