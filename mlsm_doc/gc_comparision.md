Benchmark setup:
=================================================================

db_dir: /mnt/d/mlsm/test_blob/with_gc                                                                                                                                                             ======================== Benchmark setup ========================                                                                                                                                 Job ID:                                                                                                                                                                                           Data directory:                         /mnt/d/mlsm/test_blob/with_gc                                                                                                                             WAL directory:                          /mnt/d/mlsm/test_blob/with_gc                                                                                                                             Output directory:                       /mnt/d/mlsm/test_blob/with_gc                                                                                                                             Number of threads:                      16                                                                                                                                                        Compression type for SST files:         none                                                                                                                                                      Raw database size:                      1099511627776                                                                                                                                             Value size:                             1024                                                                                                                                                      Number of keys:                         5000000                                                                                                                                                   Duration of read-write/read-only tests: 1800
Write buffer size:                      1073741824
Blob files enabled:                     1
Blob size threshold:                    0
Blob file size:                         1073741824
Compression type for blob files:        none
Blob GC enabled:                        1
Blob GC age cutoff:                     0.25
Blob GC force threshold:                1.0
Blob compaction readahead size:         0
Blob file starting level:               0
Blob cache enabled:                     1
Blob cache and block cache shared:                      1
Blob cache size:                17179869184
Blob cache number of shard bits:                6
Blob cache prepopulated:                        0
Target SST file size:                   33554432
Maximum size of base level:             268435456
=================================================================
======================== Benchmark setup ========================
Job ID:
Data directory:                         /mnt/d/mlsm/test_blob/without_gc
WAL directory:                          /mnt/d/mlsm/test_blob/without_gc
Output directory:                       /mnt/d/mlsm/test_blob/without_gc
Number of threads:                      16
Compression type for SST files:         none
Raw database size:                      1099511627776
Value size:                             1024
Number of keys:                         5000000
Duration of read-write/read-only tests: 1800
Write buffer size:                      1073741824
Blob files enabled:                     1
Blob size threshold:                    0
Blob file size:                         1073741824
Compression type for blob files:        none
Blob GC enabled:                        0
Blob GC age cutoff:                     0.25
Blob GC force threshold:                1.0
Blob compaction readahead size:         0
Blob file starting level:               0
Blob cache enabled:                     1
Blob cache and block cache shared:                      1
Blob cache size:                17179869184
Blob cache number of shard bits:                6
Blob cache prepopulated:                        0
Target SST file size:                   33554432
Maximum size of base level:             268435456
=================================================================
Use leveled compaction

without_gc:
Completed bulkload (ID: ) in 105 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
129482  128.9   62MB    2GB     1.9     0.8     50.1    38      34      0       2       7.7    8.0      19      34      231     16463   39      0.0     0       0.1     0.0     NA      bulkload2023-06-29T17:34:21     8.0.0           65dbc17110


with_gc:
Completed bulkload (ID: ) in 110 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
128267  127.7   62MB    2GB     1.9     0.8     49.6    34      30      0       2       7.8    8.0      21      41      233     17438   39      0.0     0       0.1     0.0     NA      bulkload2023-06-29T17:32:32     8.0.0           65dbc17110
