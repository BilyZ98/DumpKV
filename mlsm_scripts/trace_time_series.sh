

exe_path=../build/trace_analyzer
output_dir=/tmp/mlsm/with_gc/trace
input_trace=/mnt/nvme0n1/mlsm/test_blob/with_gc/benchmark_mixgraph.t1.s1.op_trace

if [ ! -d $output_dir ]; then
  mkdir -p $output_dir
fi
$exe_path -analyze_put=true  -convert_to_human_readable_trace=true \
  -output_time_series=true -output_dir=$output_dir -trace_path=$input_trace \
  -output_access_count_stats=true

