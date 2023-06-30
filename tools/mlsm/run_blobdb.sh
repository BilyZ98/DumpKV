#!/bin/bash
#with gc & without gc 

exe_path=../../build/db_bench 
if [ ! -f $exe_path ]; then
    echo "db_bench not exist"
    exit
fi


with_gc_db_path=/mnt/d/mlsm/with_gc
without_gc_db_path=/mnt/d/mlsm/without_gc
if [ ! -d $with_gc_db_path ]; then
    mkdir -p $with_gc_db_path
fi

num=1000000
value_size=1000
key_size=16
num_threads=1
benchmarks="fillrandom"
write_buffer_size="10485760"
user_timestamp_size=8
compression="None"
enable_blob_files=true

# data we want to collect 
# 1. throughput
# 2. write amplification
# 3. gc times
# We can get these data from db_bench output ? 
without_gc="$exe_path --db=$with_gc_db_path --num=$num \
  --value_size=$value_size --key_size=$key_size \
  --threads=$num_threads \
  --benchmarks=$benchmarks \
  --write_buffer_size=$write_buffer_size \
  --user_timestamp_size=$user_timestamp_size \
  --report_bg_io_stats=true \
  --compression_type=$compression  \
  --enable_blob_files=$enable_blob_files \
  "



# str="echo 3 | tee fuck.txt"
# $str
# $without_gc  |  tee without_gc.txt

with_gc="$exe_path --db=$without_gc_db_path --num=$num \
  --value_size=$value_size --key_size=$key_size \
  --threads=$num_threads \
  --benchmarks=$benchmarks \
  --write_buffer_size=$write_buffer_size \
  --user_timestamp_size=$user_timestamp_size \
  --report_bg_io_stats=true \
  --compression_type=$compression \
  --enable_blob_files=$enable_blob_files \
  --enable_blob_garbage_collection=true | tee with_gc.txt"

eval $with_gc
# $with_gc | tee with_gc.txt





