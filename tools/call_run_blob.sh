#!/bin/bash
function call_run_blob() {
  local wal_dir num_keys db_dir output_dir enable_blob_file enable_blob_gc
  wal_dir=$1
  num_keys=$2
  db_dir=$3
  output_dir=$4
  enable_blob_file=${5:1}
  enable_blob_gc=${6:-1}
  age_cutoff=${7:-1.0}
  compaction_trace_file=${8:-""}
  local write_buffer_size=$((100 * 1024 * 1024))
  # echo "db_dir: $db_dir"
  COMPRESSION_TYPE=none BLOB_COMPRESSION_TYPE=none WAL_DIR=$wal_dir \
   NUM_KEYS=$num_keys DB_DIR=$db_dir \
   OUTPUT_DIR=$output_dir ENABLE_BLOB_FILES=$enable_blob_file \
   ENABLE_BLOB_GC=$enable_blob_gc BLOB_GC_AGE_CUTOFF=$age_cutoff \
   WRITE_BUFFER_SIZE=$write_buffer_size NUM_THREADS=1 \
   COMPACTION_TRACE_FILE=$compaction_trace_file ./run_blob_bench.sh

 # COMPRESSION_TYPE=none BLOB_COMPRESSION_TYPE=none WAL_DIR=/tmp/test_blob \
 #   NUM_KEYS=5000000 DB_DIR=/tmp/test_blob \
 #   OUTPUT_DIR=/tmp/test_blob ./tools/run_blob_bench.sh
 
}


# with_gc and without_gc
db_dir=/mnt/nvme0n1/mlsm/test_blob
with_gc_dir=${db_dir}/with_gc
num_keys=5000000
enable_blob_file=1
enable_blob_gc=true

age_cutoff=1.0
with_gc_compaction_trace_file=${with_gc_dir}/compaction_trace.txt

call_run_blob  $with_gc_dir $num_keys $with_gc_dir $with_gc_dir $enable_blob_file $enable_blob_gc $age_cutoff $with_gc_compaction_trace_file | tee with_gc.txt
exit 0
# gdb --args  /home/bily/mlsm/db_bench --benchmarks=compact,stats --use_existing_db=1 --disable_auto_compactions=0 --sync=0 --level0_file_num_compaction_trigger=4 --level0_slowdown_writes_trigger=20 --level0_stop_writes_trigger=30 --max_background_jobs=16 --max_write_buffer_number=8 --undefok=use_blob_cache,use_shared_block_and_blob_cache,blob_cache_size,blob_cache_numshardbits,prepopulate_blob_cache,multiread_batched,cache_low_pri_pool_ratio,prepopulate_block_cache --db=/mnt/d/mlsm/test_blob/with_gc --wal_dir=/mnt/d/mlsm/test_blob/with_gc --num=5000000 --key_size=20 --value_size=1024 --block_size=8192 --cache_size=17179869184 --cache_numshardbits=6 --compression_max_dict_bytes=0 --compression_ratio=0.5 --compression_type=none --bytes_per_sync=1048576 --benchmark_write_rate_limit=0 --write_buffer_size=134217728 --target_file_size_base=134217728 --max_bytes_for_level_base=1073741824 --verify_checksum=1 --delete_obsolete_files_period_micros=62914560 --max_bytes_for_level_multiplier=8 --statistics=0 --stats_per_interval=1 --stats_interval_seconds=60 --report_interval_seconds=1 --histogram=1 --memtablerep=skip_list --bloom_bits=10 --open_files=-1 --subcompactions=1 --enable_blob_files=1 --min_blob_size=0 --blob_file_size=104857600 --blob_compression_type=none --blob_file_starting_level=0 --use_blob_cache=1 --use_shared_block_and_blob_cache=1 --blob_cache_size=17179869184 --blob_cache_numshardbits=6 --prepopulate_blob_cache=0 --write_buffer_size=104857600 --target_file_size_base=3276800 --max_bytes_for_level_base=26214400 --compaction_style=0 --num_levels=8 --min_level_to_compress=-1 --level_compaction_dynamic_level_bytes=true --pin_l0_filter_and_index_blocks_in_cache=1 --threads=1
# exit 0 
enable_blob_gc=false
without_gc_dir=${db_dir}/without_gc
without_gc_compaction_trace_file=${without_gc_dir}/compaction_trace.txt

call_run_blob  $without_gc_dir $num_keys $without_gc_dir $without_gc_dir $enable_blob_file $enable_blob_gc $age_cutoff $without_gc_compaction_trace_file | tee without_gc.txt

