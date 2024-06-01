## DumpKV: Learning based lifetime aware garbage collection for key value separation in LSM-tree

## Build 
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j16
```

## Run benchmark
Test YCSB workload for different skewness
```
cd ../tools && ./call_run_blob_multiple_ycsba.sh   zipfian-ycsba-default                                                                        
```


See the [github wiki](https://github.com/facebook/rocksdb/wiki) for more explanation.


DumpKV is developed based on RocksDB. Please see License of RocksDB.
## License

RocksDB is dual-licensed under both the GPLv2 (found in the COPYING file in the root directory) and Apache 2.0 License (found in the LICENSE.Apache file in the root directory).  You may select, at your option, one of the above-listed licenses.
