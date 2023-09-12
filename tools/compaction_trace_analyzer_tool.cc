#include "rocksdb/utilities/ldb_cmd.h"
#include <inttypes.h>
#include "db/dbformat.h"
#include "rocksdb/db.h"
#include "tools/compaction_tracer_analyzer_tool.h"
#include "rocksdb/env.h"
#include "file/writable_file_writer.h"
#include "util/coding.h"
#include "util/compression.h"
#include "util/gflags_compat.h"
#include "util/random.h"
#include "util/string_util.h"




using GFLAGS_NAMESPACE::ParseCommandLineFlags;



DEFINE_string(compaction_trace_path, "", "Path to the trace file");
DEFINE_string(compaction_output_dir, "", "Path to the output directory");
DEFINE_bool(compaction_convert_to_human_readable, true,
            "Convert the trace file to human readable format");


namespace ROCKSDB_NAMESPACE {

CompactionTraceAnalyzerTool::CompactionTraceAnalyzerTool(std::string& trace_path, std::string & output_path )
: trace_path_(trace_path), output_path_(output_path) {
  env_ = Env::Default();
  env_options_ = EnvOptions();
  // trace_reader_ = std::unique_ptr<TraceReader>(new TraceReader(env_, trace_path, env_options_));
  // compaction_trace_reader_ = std::unique_ptr<CompactionTraceReader>(new CompactionTraceReader(trace_reader_.get()));
  // compaction_trace_reader_->Read

}



Status CompactionTraceAnalyzerTool::PreProcessing() {

  Status s;
  if(trace_reader_ == nullptr) {
    s = NewFileTraceReader(env_, env_options_, trace_path_, &trace_reader_);
    // trace_reader_ = std::unique_ptr<TraceReader>(new TraceReader(env_, trace_path_, env_options_));
  } else {
    s = trace_reader_->Reset();
  }


  if(!s.ok()) {
    return s;
  }
  


  if(FLAGS_compaction_convert_to_human_readable) {
    std::string human_readable_output = output_path_ + "/compaction_human_readable_trace.txt";
    s = env_->NewWritableFile(human_readable_output, &trace_sequence_f_, env_options_);
    if(!s.ok()) {
      return s;
    }
  }

  return Status::OK();

}



Status CompactionTraceAnalyzerTool::StartPorcessing() { 
  CompactionTraceReader compaction_trace_reader(std::move(trace_reader_));
  Status s ;
  CompactionTraceRecord record;

  while(s.ok()) {
    s = compaction_trace_reader.Read(&record);
    if(!s.ok()) {
      break;
    }
    if(FLAGS_compaction_convert_to_human_readable && trace_sequence_f_ != nullptr) {
      int ret;

      Slice ikey(record.drop_key);
      ParsedInternalKey parsed_key;
      s = ParseInternalKey(ikey, &parsed_key, true);
      if(!s.ok()) {
        assert(false);
        return s;
      }
      if(parsed_key.sequence == 0) {
        fprintf(stderr, "key: %s, sequence number is 0\n", parsed_key.user_key.ToString().c_str());
        assert(false);
      }
      std::string hex_user_key = ROCKSDB_NAMESPACE::LDBCommand::StringToHex(parsed_key.user_key.ToString()); 
      ret = snprintf(buffer, sizeof(buffer), "%" PRIu64 " %" PRIu64"\n",  parsed_key.sequence, record.timestamp);
      if(ret < 0) {
        return Status::IOError("cannot write to the human readable trace file");
      }
      std::string printout(buffer);

      printout = hex_user_key  + " "+ printout    ;
      s = trace_sequence_f_->Append(printout);
    } else {
      fprintf(stderr, "convert_to_human_readable is false or trace output file is null\n");
      break;
    }
  }

  if(s.IsIncomplete()) {
    return Status::OK();
  }
  return s;
}

int compaction_tracer_analyzer_main(int argc, char** argv) {
  std::string trace_path ;
  std::string output_dir ;
  ParseCommandLineFlags(&argc, &argv, true);

  CompactionTraceAnalyzerTool compaction_trace_analyzer_tool(FLAGS_compaction_trace_path, FLAGS_compaction_output_dir);
  Status s;
  s = compaction_trace_analyzer_tool.PreProcessing();
  if(!s.ok()) {
    fprintf(stderr, "%s\n", s.getState());
    fprintf(stderr, "cannot initiate the tracer reader\n");
    exit(1);
  }

  s = compaction_trace_analyzer_tool.StartPorcessing();
  if(!s.ok()) {
    fprintf(stderr, "%s\n", s.getState());
    fprintf(stderr, "cannot start processing the trace\n");
    exit(1);
  }
  return 0;
}





}
