#!/bin/bash
function call_run_blob() {
  local wal_dir num_keys db_dir output_dir enable_blob_file enable_blob_gc
  wal_dir=$1
  num_keys=$2
  db_dir=$3
  output_dir=$4
  enable_blob_file=${5:1}
  enable_blob_gc=${6:-1}
  age_cutoff=${7:-1}
  local write_buffer_size=$((100 * 1024 * 1024))
  echo "db_dir: $db_dir"
  COMPRESSION_TYPE=none BLOB_COMPRESSION_TYPE=none WAL_DIR=$wal_dir \
   NUM_KEYS=$num_keys DB_DIR=$db_dir \
   OUTPUT_DIR=$output_dir ENABLE_BLOB_FILES=$enable_blob_file \
   ENABLE_BLOB_GC=$enable_blob_gc BLOB_GC_AGE_CUTOFF=$age_cutoff \
   WRITE_BUFFER_SIZE=$write_buffer_size ./tools/run_blob_bench.sh

 # COMPRESSION_TYPE=none BLOB_COMPRESSION_TYPE=none WAL_DIR=/tmp/test_blob \
 #   NUM_KEYS=5000000 DB_DIR=/tmp/test_blob \
 #   OUTPUT_DIR=/tmp/test_blob ./tools/run_blob_bench.sh
 
}


# with_gc and without_gc
db_dir=/mnt/d/mlsm/test_blob
with_gc_dir=${db_dir}/with_gc
num_keys=5000000
enable_blob_file=1
enable_blob_gc=true


call_run_blob  $with_gc_dir $num_keys $with_gc_dir $with_gc_dir $enable_blob_file $enable_blob_gc

enable_blob_gc=false
without_gc_dir=${db_dir}/without_gc

call_run_blob  $without_gc_dir $num_keys $without_gc_dir $without_gc_dir $enable_blob_file $enable_blob_gc

