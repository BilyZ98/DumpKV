

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
    breakpoint:compaction_trace_analyzer_tool.cc:80
    key_length:28




code position to check how sequence is appended to each key: db_impl_write.cc:526

Plan to write the start sequence of write batch to trace and construct 
the internal key when reading the trace.
Another thing is that how do I know the type field of internal key ? 


    
I think I can get some insight about how to get internal key by inspecting 
the code WriteBatchInternal::InsertInto() at db_impl_write.cc:560


I don't think we need type ? 
We can do more data engineering after we get the key with sequence number
we can strip the type from internal key in the compaction trace data.
