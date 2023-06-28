#!/bin/bash
#with gc & without gc 

exe_path=../../build/db_bench 
if [ ! -f $exe_path ]; then
    echo "db_bench not exist"
    exit
fi


with_gc_db_path=/mnt/d/mlsm/with_gc
without_gc_db_path=/mnt/d/mlsm/without_gc

num=10000000
value_size=1000
key_size=16
num_threads=1
benchmarks="fillrandom"
write_buffer_size="1048576"
user_timestamp_size=8


with_gc="$exe_path --db=$with_gc_db_path --num=$num \
  --value_size=$value_size --key_size=$key_size \
  --num_threads=$num_threads \
  --benchmarks=$benchmarks \
  --write_buffer_size=$write_buffer_size \
  --user_"

