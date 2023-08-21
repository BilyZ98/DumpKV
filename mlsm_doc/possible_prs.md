1. db_bench_tool.cc
    add flags to control trace file creation for benchmark methods.

2. db_impl_write.cc 
    add tracer write code for condition !tracer_->IsWriteOrderPreserved()


3. new: compaction_tracer.cc 
    add compaction_tracer to trace droppped keys.


4. trace_analyzer_tool.cc 
    add sequence tracer
