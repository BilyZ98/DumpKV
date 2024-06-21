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

## Paper link
[DumpKV: Learning based lifetime aware garbage collection for key value separation in LSM-tree](https://arxiv.org/pdf/2406.01250)
Cite our paper if it's helpful to your work
```
@misc{zhuang2024dumpkv,
      title={DumpKV: Learning based lifetime aware garbage collection for key value separation in LSM-tree}, 
      author={Zhutao Zhuang and Xinqi Zeng and Zhiguang Chen},
      year={2024},
      eprint={2406.01250},
      archivePrefix={arXiv},
      primaryClass={id='cs.DB' full_name='Databases' is_active=True alt_name=None in_archive='cs' is_general=False description='Covers database management, datamining, and data processing. Roughly includes material in ACM Subject Classes E.2, E.5, H.0, H.2, and J.1.'}
}
```

See the [github wiki](https://github.com/facebook/rocksdb/wiki) for more explanation.


DumpKV is developed based on RocksDB. Please see License of RocksDB.
## License

RocksDB is dual-licensed under both the GPLv2 (found in the COPYING file in the root directory) and Apache 2.0 License (found in the LICENSE.Apache file in the root directory).  You may select, at your option, one of the above-listed licenses.
