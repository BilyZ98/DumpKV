

Goal: Identify what type of keys trace_analyzer records

Action:
gdb trace_analyzer 
    filename:/mnt/nvme0n1/mlsm/test_blob/with_gc/benchmark_mixgraph.t1.s1.op_trace
    breakpoint:trace_analyzer_tool.cc:1674
    key_length:20
    initial insert length:20

This means op tracer records user key?
gdb compaction_analyzer 
    filename:/mnt/nvme0n1/mlsm/test_blob/with_gc/compaction_trace.txt
    breakpoint:
    key_length:
    
