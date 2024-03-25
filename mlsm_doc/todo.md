
Read this doc to learn about the api and features in rocksdb
https://rocksdb.org/blog/2021/05/26/integrated-blob-db.html


## API 
`enable_blob_files`: set it to true to enable kv separation
`enable_blob_garbage_collection`: set this to this to true to make BlobDB actively 
relocate valid blobs from the oldest blob files as they are encountered 
during compaction
`blob_garbage_collection_age_cutoff`: the threshold that the GC logic uses to 
determine which blob files should be considered "old".

The above options can be changed via SetOptions API

Use Leveled Compaction style combined with BlobDB


Lifetime for each key. 
How many keys are GCed during one GC ? 


## File related to GC 
What is Blob ? A file ? 
Does Rocksdb GC a whole file instead of individual keys ? 
db/compaction/compaction_iterator.cc
    CompactionIterator::PrepareOutput()
    CompactionIterator::GarbageCollectBlobIfNeeded()

Each time CompactionIterator::Next() is called  CompactionIterator::PrepareOutput()
 is called to write large value to blob or GC blob value.

 There is no lifetime statistics for each key right now. 
 Let's do it .

Rocksdb has already implemented that timestamp can be added to internalkey.
I can use this

How can I enable timestamp to be written to internalkey when Flush is scheduled?
db/db_with_timestamp_test_util.h
Check db_bench help. We can use ComparatorWithU64TsImpl
But I am not sure if Flush will write timestamp

Want to find that how we can add timestamp to each internal key
flush_job.cc
FlushJob::WriteLevel0Table()
    Status BuildTable()

Still don't find out how timestamp is written when key is put. Start from the top
function.
Timestamp is written as part of the key when DB::Put() is called.
So now I think we can collection information about the lifetime of each key during 
compaction. 

one thing I don't  know is that how many times of compaction through which the key 
has to go before it's actually GCed.

Rocksdb choose a GC method that's different from that in Wisckey. GC is perfromaned 
during compaction. 
Does this mean that each SSTable has a corresponding Blob ? So we may still have 
large write amplification. 
This feels not right. 
I need to do check about this .


Compaction code is in ProcessKeyValueCompaction() function 
We have BlobBuilder and CompactionIterator here.


CompactionJobStat is a struct we can use to do statistics about lifetime for each key
in one compaction.


What does force threshold mean? 
Need to figure out how these parameters work.

Trying to figure out how GC can be triggered. 
Right now I am not seeing GC triggered.


Only see kForcedBlobGC CompactionReason.

Read LOG and figure out that I can search log keywords in the codebase to see
 how GC can be triggered.

Can't figure our.
Let's set enable_blob_garbage_collection=1.0 to see if GC can be triggered with 100%
cerntainty.


compacting code:
```
gdb --args  /home/bily/mlsm/db_bench --benchmarks=compact,stats --use_existing_db=1 --disable_auto_compactions=0 --sync=0 --level0_file_num_compaction_trigger=4 --level0_slowdown_writes_trigger=20 --level0_stop_writes_trigger=30 --max_background_jobs=16 --max_write_buffer_number=8 --undefok=use_blob_cache,use_shared_block_and_blob_cache,blob_cache_size,blob_cache_numshardbits,prepopulate_blob_cache,multiread_batched,cache_low_pri_pool_ratio,prepopulate_block_cache --db=/mnt/d/mlsm/test_blob/with_gc --wal_dir=/mnt/d/mlsm/test_blob/with_gc --num=5000000 --key_size=20 --value_size=1024 --block_size=8192 --cache_size=17179869184 --cache_numshardbits=6 --compression_max_dict_bytes=0 --compression_ratio=0.5 --compression_type=none --bytes_per_sync=1048576 --benchmark_write_rate_limit=0 --write_buffer_size=134217728 --target_file_size_base=134217728 --max_bytes_for_level_base=1073741824 --verify_checksum=1 --delete_obsolete_files_period_micros=62914560 --max_bytes_for_level_multiplier=8 --statistics=0 --stats_per_interval=1 --stats_interval_seconds=60 --report_interval_seconds=1 --histogram=1 --memtablerep=skip_list --bloom_bits=10 --open_files=-1 --subcompactions=1 --enable_blob_files=1 --min_blob_size=0 --blob_file_size=104857600 --blob_compression_type=none --blob_file_starting_level=0 --use_blob_cache=1 --use_shared_block_and_blob_cache=1 --blob_cache_size=17179869184 --blob_cache_numshardbits=6 --prepopulate_blob_cache=0 --write_buffer_size=104857600 --target_file_size_base=3276800 --max_bytes_for_level_base=26214400 --compaction_style=0 --num_levels=8 --min_level_to_compress=-1 --level_compaction_dynamic_level_bytes=true --pin_l0_filter_and_index_blocks_in_cache=1 --threads=1
```


Switch to common testbed of nscc because my wsl was stuck when I try to run benchmark
that start 16 threas.
I don't know what's going on and how to find out the root cause for now. 

I will just skip that and switch to a new environment, hope this can solve the issue.



Get first write amplification comparison between garbage collection 
enabled and disabled rocksdb.
It proves my assumption that current garbage collection is not smart enough.

It's hard to find a good static parameters that can efficiently garbage collect 
outdated keys with low write amplification .


I will then plot a figure about the write amplification and the key distribution
And also the space amplification. 


So, how to get these data ? 



How can I demonstrate that this is problem with histogram or statistics image?

Considering using YCSB and mixgraph workload.
someone has already write a script to get the key metrics.
It's `benchmark.sh` under tools 

Also , once thing I need to do to demonstrate the advantage of the using machine learning 
is that the I need to achieve better write amplification compared to manually setting 
parameter blob_gc_age_cut_off

Also, current solution of garbage collection cannot GC recent hot writing keys
how can I demonstrate this ?


Let's read some paper now .


How can I deal with cold start ? 
Do I need to use offline learning ? 


There is difference between cache and the lifetime of keys 


So how can we collect the lifetime data to train the model ? 
What's the most naive way to collect data ? 
Trace information ?


Let's just use offline training now. 
If we have good results then we can come up with new ways to 
use online learning .


I need to write a script or function to  gather lifetime of each key .


If I get the lifetime of the key, what's my strategy to arrange the 
blob values when GC happens ? And Can I also prioritize the GC time ?

So the model need to learn the lifetime distribution of keys.
To simplify the prediction task we can do classification of lifetime
of keys. 
We can give two classifications which are short and long. How to quantify
short and long ? 


The ideal solution is that model can predict the exact lifetime of 
keys. 


I have a sense of urgency and I need to be fast in case other 
two APs to whom I share my ideas to working faster..

Find out why trace_analyzer doesn't output time_series data



Was thinking about adding another trace that can collect what keys 
are GCed or involved during compaction. 

Currently we don't know the when the deleted  keys in LSM-tree are actually 
removed.


Use trace information from mixgraph workload and get that 50% of
the keys are conly accessed once .
I will adjust the distribution parameters to get more keys 
accessed at least twice .


So I can add another function in trace_analyzer to gather the lifetime of 
each time.

Get the avg lifetime statistics histogram, I also need to draw cumulative density
figure to see the percentage of the low lifetime keys.

Learn one gnuplot command to draw  histogram that counts the occurrences of 
same x values.
```
binwidth = <something>
bin(val) = binwidth * floor(val/binwidth)
plot "datafile" using (bin(column(1))):(1.0) smooth frequency 

```


Now let's learn how to how probability density funciton from the data.
Just single column of data. 
cumulative distribution function 
https://stackoverflow.com/questions/1730913/gnuplot-cumulative-column-question
```
stats file
plot file using 2:(1.0) smooth cumulative title "cdf "
plot file using 2:(1.0/STATS_records) smooth cumulative title "cdf"
```


know more about the smooth option in gnuplot to do the filtering and grouping 
One can also preprocess the data to the format they want and not use these 
options. But it's convenient. 


So I guess now I have to write code to get the time when they key is actually 
GCed during compaciton. 
I can add another tracer called compaction tracer.

So where is the key being GCed exactly ? Let's find out .
CompactionJob::ProcessKeyValueCompaction
Reading all the code of  ProcessKeyValueCompaction function. 
Just try to learn something . The C++ standard usage.
std::optional<>  this is like rust option 


Try to know what is CompactionFilter and how CompactionFilter connects with 
obsolete keys.

I am serching for how keys are dropped by searching the keyword 'num_record_drop_obsolete'
The effect of dropping keyowrds of This keyword seems only applying to deletion operation,
how about put operation ? 


Seems like this is the part where obsolete put values are discarded
```
          } else if (has_outputted_key_ ||
                     DefinitelyInSnapshot(ikey_.sequence,
                                          earliest_write_conflict_snapshot_) ||
                     (earliest_snapshot_ < earliest_write_conflict_snapshot_ &&
                      DefinitelyInSnapshot(ikey_.sequence,
                                           earliest_snapshot_))) {
            // Found a matching value, we can drop the single delete and the
            // value.  It is safe to drop both records since we've already
            // outputted a key in this snapshot, or there is no earlier
            // snapshot (Rule 2 above).

            // Note: it doesn't matter whether the second key is a Put or if it
            // is an unexpected Merge or Delete.  We will compact it out
            // either way. We will maintain counts of how many mismatches
            // happened
            if (next_ikey.type != kTypeValue &&
                next_ikey.type != kTypeBlobIndex &&
                next_ikey.type != kTypeWideColumnEntity) {
              ++iter_stats_.num_single_del_mismatch;
            }

            ++iter_stats_.num_record_drop_hidden;
            ++iter_stats_.num_record_drop_obsolete;
            // Already called input_.Next() once.  Call it a second time to
            // skip past the second key.
            AdvanceInputIter();
 
```

Why does  compaction_iterator use has_current_user_key_ and at_next_ ? 
What's the difference between AdvanceInputIter() and NextFromInput()?



So now I will find the place where I can record the obsolete keys and write a new class
to record these keys.


Met a if statement in NextFromInput() of compaction logic that I don't understand
So I check the logics of comparison of two slice with timestamp and 
find that two keys a and b with same user key will have the key with 
younger timestamp(bigger numerically) tagged smaller which means the older time stamp if 
put ahead of younger timestamp.
So during compaction the key with older timetamp is fetched first.
```
    int Compare(const Slice& a, const Slice& b) const override {
      int r = CompareWithoutTimestamp(a, b);
      if (r != 0 || 0 == timestamp_size()) {
        return r;
      }
      return -CompareTimestamp(
          Slice(a.data() + a.size() - timestamp_size(), timestamp_size()),
          Slice(b.data() + b.size() - timestamp_size(), timestamp_size()));
    }
```


Get more knowledge about the key_ fields in CompactionIterator struct 
so `key_` is 


Why current_key_committed_ in compaction process ? I will just skip this for now 

Forget about this, I have spent enough time looking through the very first part of
the compaction iterator code . Now I will focus on when the key is dropped. 

My progress is slow.


So we only need to add the dropped keys to compaction tracer here.
In CompacitonIterator.
```
CompactionIterator {
private:

  // Processes the input stream to find the next output
    NextFromInput()
}
```

Is the key written in to tracefile internalkey or userkey ?
If it's a internal key then it is easy to track the lifetime .
If it's userkey, then we may have to modify op tracer 
to turn userkey into internal key because internalkey is unique.


Write test suite to test compaction tracer by imitate block cache tracer test.
I will finish compaction tracer test and collect my first drop keys tracer
information tomorrow. 


Also I will do a check about the key type in op trace file.
Use microsecond as timestamp is enough I think.


Finding a way to insert compaction tracer and collect trace information
How about insert 

db/db_impl/db_impl_compaction_flush.cc
    DBImpl::BackgroundCompaction()
    DBImpl::MaybeScheduleFlushOrCompaction()

compaction_job.cc
    CompactionJob::Run()


I can insert compaction into VersionSet and add another field in options.
since immutable_db_options is passed to CompactionJob I can add this field to 
immutable_db_options .



Since we have multiple subcompaction I don't know if we can store a pointer 
to compactiontracer in each subcompaction job .
I think it can .



keys dropped reason marked as  `num_record_drop_user` or `num_record_drop_hidden`
or `num_record_drop_obsolete` should be recorded by compaction_tracer_;


I need to run a gdb to see if my assumption that last_snapshot == current_user_key_snapshot_
is true
It doesn't make sense to me now. 


There is certainly some time we have to spend to deal with system running 
and code debugging


Let's just gather the trace information and then decide how we can use this information 
to do a better gabarge collectino process. 

There is no compaction trace gathered ! :(
Let's see why .


I see, I did not write the compaction trace by calling Write method of compaction tracer.
Let's do that now .
Oh no, just realized that I have called the  WriteDropKey() of compaction_tracer_


Debug the code for three days and now I realize that I didn't not pass the compaction tracer pointer to compaction job as it seems.
Let's do it now .
It's that the flush job that doesn't get a compaction tracer but uses compaction iterator that 
causes the nullptr reference error.
Fixing it. 


Found it , the old version of the same key will be dropped by compaction iterator 
and marked as hidden drop.



I get the human readable trace file for dropped keys during compaction which means 
I get the timestamp for the dropped keys but I don't get the timestamp when keys
are flushed to LSM-tree.

I think I need to combine compaction trace and the op trace together to get the 
insert timestamp and the dropped timestamp.
Let's do it.


Four steps in  op trace analyzer.
1. preparprocessing -> open output file
2. start processing read all the trace records into memory
3. make statistics, assign key id to each key and get the access count 
4. reprocessing 
4. endprocessing , close output file


I can do a simple check to see if keys recorded in op trace file is the 
same as internal keys which is checking the length of the keys.
If the length of the keys from both trace files are the same, then 
they are the same type of keys
I can use gdb for this check.. 


Then, how can I combine trace information of compaction and op.
I will do most easy ways. Just insert the code in to the trace analyzer.



Let's check if startTracer will overwrite existing trace file.
Check the code. db_bench doesn't check use_existing_db flag when 
start trace which means it will overwrites previous file.
I don't know if I should raise a pr about this.
Because users can put all the methods they want to call in benchmark flags.

Turns out it's user key that trace analyzer records.
The benefit of this is that we can get the exact time the same key is rewriteen
The downside is that we can't get the exact time when this key is garbage collected
in the compaction or garbage collection process.


How about we rewrite trace collection logics so that internal keys will be 
rewritten to op trace file and we can further get user key from internal key
later in the processing pipeline..


Need to figure out a plan to get the time when user key is obsolete and when this 
user key is actually garbage collected.


I will do the statistics of the lifetime now. I think I've done it before.

Let me try different workload.
[Highlight]
### And then I can get the data about the garbage collection performance of different strategies for different workload.


How does sequence number work with operations other than put suchs as delete and deleterange.
Looks like it's basically the same for put, delete and singledelete except for deleterange
Two sequence numbers are used for begin and end of the range key for deleterange


Advacement of sequence is done by MemTableInserter. However, we've already know that in
advance that how many sequence  number will be used before memtable inserter isused.

Since we only use benchmarks without deleterange, it ok to have the assumption that we have 
one sequence number for each op or key in the writebatch trace


So now let's change the code about reading trace and do some test.
 I will continue this after I finish my texas holdem game.

After spent long time(1hr to 2hrs) checking the trace_analyzer code , figure out 
how put op trace information is written to final output files.

Need to implement DeleteCFWithStartSequence()


I need to write a test to verify that sequence number extracted from op trace 
and compaction trace is the same.


~~I just realize that compaction_trace_analyzer does not map key to key_id like op_trace_analyzer 
does.~~ 
Human readable file contains original key slice trace information instead of key_ids recorded in time_series 
file. However, keys are shown in hex format in human readable trace file of op trace analyzer.


phew, pass the compaction_op_trace_analyzer test.
So this proves that our compaction tracer collects the same user key and sequcen number 
as our op tracer.

Our next step is to get the lifetime of userkey and lifetime of internal key.


Write another test for compaction_op_trace_analyzer test suite which introudces put and delete ops.
It passes .

So now I guess it's time to do statistics about the lifetime and then I can start 
test the model prediction accuracy and performance.

I am getting excited about this move after spending such long time writing trace 
collection module..




Considering adding a new flag to db_bench to let users of db_bench decide if they want a trace file 
for all benchmarks or they want a single trace file for each benchmark.

Fix the db_bench_tool.cc so that tracer use the same file for all benchmark methods 
in one command line instead of create a new file for each method. 
I can submit a pr to rocksdb based on my current solution.



What's PipelinedWriteImpl()? 
There is no !write_order_preserved option in PipelinedWriteImpl() which is
different than that in WriteImpl() .


How can I get the sequence number for tracer before WriteMemtable or WriteImplWALOnly()?
Maybe I should just skip this part ? 
This is not so important compared to my research project.


Spend all whole day reading garbage collection code logic because I have a question when I was
writing scripts to calculate the lifetime of each key. I was thinking about if there is difference 
between actual lifetime and actual gc time for each value in blob files.
Because values in blob files are rewritten to new blob files when corresponding keys in sst
files are traversed.
So it can happen that values stay much longer although it is gced during compaction process.
Those values are not gced actually because the corresponding blob files are not deleted.

So I guess I need to write code to treat keys differently with respect to 
which blob files they come from. If keys gced come from blob files being gced, then 
these kvs are gced actually, otherwise, there will be garbage in blob files that are not involved in
gc process.
However, this is not important for now. What we care is the lifetime of obsolete keys. 

What else information I need to gather ? 
Write amplification, space amplification.
Average lifetime ?  Mean lifetime, lifetime variance.  




Two ways to show current gc is not efficient enough
1. From user perspective , lifetime difference between actual lifetime and valid lifetime
    1.2. Write amplification, space amplification,  
    Hot user keys usually come with short lifetime.
    Cold user keys can have different lifetime. The interval between two puts of the same user key
    determines the lifetime of the user key. 
    So there can be huge lifetime difference in cold keys.
    This is different than cache problem which usually considers hotness.
    For cache problem, we need to put keys that are hot into cache and evict the cold ones. There is limited space, we need to
    allocate precious space to imporant ones.
    For lifetime problem, all keys share same importance in terms of storage space. 
    The ideal situation is that we put keys with short lifetime together and we 
    put keys with long lifetime together. And then we can schedule gc with trigger based method 
    or some other methods.

    One quesiton, can we really learn the short lifetime of keys that are cold? 
    It's easy to learn the lifetime of hot keys . It will still be good if we can 
    get a high prediction acc in hot keys..

3. From internal perspective,we can get a holistic view of statistics of internal obsolete keys
    to show that each compaction or gc only pick up small propotion of obsolete keys.



I need to show that this is a big problem.  So now I am running workload with different 
gc parameters in rocksdb. 


Current space amplification calculation is not accurate. I need to use space amplificaiton
metrics of blob files to show that current param based garbage collection method is not efficient 

Let's check how db_bench  get space amplification for blob files


Again as I say,  current space calculation method in rocksdb db can not reflect the true space amplification
of the  whole db. Because it does not know whether the valid ones in blob files 
are obsolete or not. 


So I have two main work items now. 
1. continue with adding new metrics and code to gather this metrics to evalute write amplification, space amplification
, and key lifetime of all keys.
2. Train model to predict the lifetime . Come up with solution to predict lifetime of keys.
    Where does data come from?
    What kind of model should we use ? 
    How can we evalute the performance of the model?
    How does the model interact with LSM-tree?



internal_key_lifetime.py
How about running another workload. I learn that I can use sysbench yesterday 
to create skewed workload.
Currently the mixgraph workload generate the key lifetime distribution in which 
60% of keys have lifetime 1.2e8 which is 25% of the longest lifetime.
Current valid_duration is calculated for each internal key instead of user key.
However, I don't do statistics for each user key now. It's worth doing. 
It may happen that for the same user key, the lifetime for each put is different. 
So then it comes to the question that how can we train the model to predict the


Let's try the LightGBM model mentioned in the Learper paper.
The output of the model will be the approximation lifetime of this key.



Read a Learned cache paper called GLCache which group cached object into level.
I learn what features DumpLSM can use to predict the lifetime of keys.
Here's the following features DumpLSM can use.
1. Write time
2. Key size 
3. Content type? 
4. Read/Write ratio

GLCache also shows a coefficient between objects for different group size. 
I wonder how does it do the calculation. It would be helpful if I do this too.






# Model
LightGBM 
## Parameter tuning 
https://lightgbm.readthedocs.io/en/stable/Parameters-Tuning.html
What's the approach to tune parameters used in Leaper paper? 
I remember that Leapers uses automatic paramter searching.
GridSearchCV from scikit-learn 
New area to dive in. Whoo ray!
https://github.com/microsoft/FLAML for auto matic parameter searching

XGBoost , CatBoost, 

LRB model to do object level learning 
github: https://github.com/sunnyszy/lrb

Learned index for variable lenght input
https://dspace.mit.edu/bitstream/handle/1721.1/144902/Spector-spectorb-meng-eecs-2022-thesis.pdf?sequence=1&isAllowed=y


So for this task we focus on fixed size integer input. And I will convert 
input key to integer.


- Todo: get valid duration for keys that are not in compaction but is overwritten.


There is sequence number 0 in compaction trace file which is not supposed to show up.
I need to do some check to see if this is normal.


Current compaction tracer writes internal key at compaction_iterator.cc
I don't think I make any changes to the internal keys during write or read time.
But why does zero sequence number show up? 



https://groups.google.com/g/rocksdb/c/iLy38REYWtY?pli=1
This discussion give explanation why zero sequence number shows up in
internal keys. However, there is no compacting to bottommost levels .


I see, it looks like  that the sequence number may be set to zero during 
compaction or during put operation.
If rocksdb makes sure that there is no such key in the higher level then it
can set sequence number to zero during compaction process.

I am not sure about this.
How can I verify this ?


So now I am checking the Write() and PipelinedWriteImpl() in db_impl_write.cc 
to see what's wrong.  It may be that the sequence number is zero in Write().
The op tracer assumes that each key in put has a unique asscendent sequence number.
This assumption may be not correct.

Run my first machine learning model prediction using only key and insertion time
The prediction target is the lifetime of each key.
So we ahve only two features for this regression task. 
The regression task is hard.

Here's result
```

[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.091627 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 510
[LightGBM] [Info] Number of data points in the train set: 213626, number of used features: 2
[LightGBM] [Info] Start training from score 24491986.889803
Starting predicting...
The rmse of prediction is: 19275429.20166085
```
std of california housing price? 
```
std_dev:  1.1539282040412253
Starting training...
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.084580 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 1838
[LightGBM] [Info] Number of data points in the train set: 16512, number of used features: 8
[LightGBM] [Info] Start training from score 2.072499
Starting predicting...
The rmse of prediction is: 0.7133873256387498
```
std of housing price drops about 40% after training

std of lifetime of keys drops about 13% after training

Can't say this is a good regression model
The bing chat tells me that I can assess the model regression performance by 
checking if the rmse is lower than std of the dataset.
The std of the dataset is :std_dev:  22298492.15679915 
So the regression model do have some help in terms of lifetime regression but not so impressive.
The goal of this task is to reduce write amplification and space amplification.


So I have two direction to optimize this task.
1. From the features side
    Use more features such as read/write rate. rocksdb information of different levels

2. From task side
    Change the task from regression to classification.
    We need to more data engineering such that we need to give a classification 
    to each lifetime in the dataset we collect.



I need to build a evaluation pipeline to test how the model affect the write
amplification and space amplification.
But before that we need to integrate the model into the rocksdb.

rmse of model perfomrance :https://datascience.stackexchange.com/questions/9167/what-does-rmse-points-about-performance-of-a-model-in-machine-learning
LightGBM model have function to calculate the importance of each feature.


What should I do now?

Read write rate  should be given a try.
We can just collect write arribal rate for now due to our task attribute..

I need to make sure current sequence number collectoion logics is correct.
The sequence number in op tracer should be exactly what it is of the actual 
internal key.

compaction_outputs.cc
```
Status CompactionOutputs::AddToOutput(
    const CompactionIterator& c_iter,
    const CompactionFileOpenFunc& open_file_func,
    const CompactionFileCloseFunc& close_file_func) {
 
```
In this function num_output_records is changed when there is new output
to be added to new files..

CompactionIterator decides whether the key should be dropped.
Looks like the drop type is drop_hidden. The location of drop is at line 834 
in file compaction_iterator.cc


Understand how keys are dropped during compaction. Snapshot number is used
to drop the internal key that share same user key but with smaller sequence number.

Internal keys with zero sequence numer are observed  during compaction.
I don't think the sequence number of internal keys are changed during 
the compaction process. So it's very likely that the sequence number is zero 
when it is first written to MemTable or duing Flush process.
But why ?

This is important . Becuase if sequence numbers in op trace and compaction does not align
then we can't get the true lifetime data.

Now I know that there is  mempurge process when Flush happens.
The mempurge process will do a garbage collection during Flush in memory .
Does this affect the sequencen number of internal keys?


flush_job.cc
line:817
```
Status FlushJob::WriteLevel0Table() {
    s = BuildTable()
```


builder.cc
```
Status BuildTable(
    const std::string& dbname, VersionSet* versions,
    const ImmutableDBOptions& db_options, const TableBuilderOptions& tboptions,
    const FileOptions& file_options, TableCache* table_cache,
    InternalIterator* iter,
    std::vector<std::unique_ptr<FragmentedRangeTombstoneIterator>>
        range_del_iters,
    FileMetaData* meta, std::vector<BlobFileAddition>* blob_file_additions,
    std::vector<SequenceNumber> snapshots,
    SequenceNumber earliest_write_conflict_snapshot,
    SequenceNumber job_snapshot, SnapshotChecker* snapshot_checker,
    bool paranoid_file_checks, InternalStats* internal_stats,
    IOStatus* io_status, const std::shared_ptr<IOTracer>& io_tracer,
    BlobFileCreationReason blob_creation_reason,
    const SeqnoToTimeMapping& seqno_to_time_mapping, EventLogger* event_logger,
    int job_id, const Env::IOPriority io_priority,
    TableProperties* table_properties, Env::WriteLifeTimeHint write_hint,
    const std::string* full_history_ts_low,
    BlobFileCompletionCallback* blob_callback, Version* version,
    uint64_t* num_input_entries, uint64_t* memtable_payload_bytes,
    uint64_t* memtable_garbage_bytes) {

```

Find the place where sequence number of internal key is set to zero.
It's in compaction_iterator.cc
```
void CompactionIterator::PrepareOutput() {
```
It says that zeroing out the sequence number leads to better compression.
If current compaction level is the bottommost_level_ which means there is 
no files at levels higher than current level

This sequence zero feature affects my compaction tracer.
Because compaction tracer need to align with op trace on sequence number.


I can submit a pr about compaction tracer later with another option of 
not zeoring sequence number.

But I need to make sure sequence number align with each other both in
op trace file and compaction trace file.
I can write the trace to the op trace file during MemtableInserter iteration.


So sequence number is integrated into internal key at PutCFImpl()  of 
MemTableInserter
like this at write_batch.cc 
```
mem->Add(sequence_, value_tyep, key, value,...)

```
MemTable is incharge of put user key and sequence number together.


It looks like the sequence number in op trace file may not align with that in 
compaction trace file. Because in pipelined write implementation WAL group
and memtable group is not necessary has the same order of keys.


But since we are only writing in one thread so the sequence number should align
in op trace file and compaction trace file


Can we get read write ratio in rocksdb statistics module?

Let's check the code tomorrow.

Let's check it  now.

In file db.h
```
  virtual bool GetProperty(const Slice& property, std::string* value) {
    return GetProperty(DefaultColumnFamily(), property, value);
  }


```

We can call GetProperty() function to get write ratio and 
other db statistics.
Let's see if we can get write ratio in a period of time

Find it. We can call GerProperty() 
to get DB Stats and Interval writes


So now let's run our internal_key_lifetime script to get lifetime .
We need to make changes to our current internal_key_lifetime script to 
see how many keys are still alive. Maybe not now. 
I need to use gxr's script to gather true lifetime of all keys instead of
keys that are involved in compaction.

DB stats example

```
** DB Stats **
Uptime(secs): 180.1 total, 60.1 interval
Cumulative writes: 1714K writes, 1714K keys, 1714K commit groups, 1.0 writes per commit group, ingest: 1.69 GB, 9.62 MB/s
Cumulative WAL: 1714K writes, 1714K syncs, 1.00 writes per sync, written: 1.69 GB, 9.62 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 557K writes, 557K keys, 557K commit groups, 1.0 writes per commit group, ingest: 563.20 MB, 9.38 MB/s
Interval WAL: 557K writes, 557K syncs, 1.00 writes per sync, written: 0.55 GB, 9.38 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent
num-running-compactions: 0
num-running-flushes: 0


```

Maybe we don't need key's id as feature for model  . 
Let's use it for now. 


Two work items
1. Get internal key lifetime for different paramters and different worklaod
2. Write write rate to op trace to gather more feature for training


Let's run the workload first.

I will integrate the model into the rocksdb after I add the write ratio 
features into data.

Classify lifetime into different categories ? Difinitely good idea.
Will do it after I get the write ratio features.

Maybe I can come up with algorithm to classify lifetime into different categories.
Let's make the assumption that we've already have a algorithm to know 
how many classes should have for all keys. 

Third work item
3. Integrate the model intor rocksdb 

I will do integration after get the write rate features into trace file as train data

Scripts to be run after the db_bench  to evalute the metrics
mlsm_scripts/internal_key_lifetime.py:  get lifetime
tools/calculate_user_data.py:  get space amplification 
tools/mlsm_plot_metrics.py: get write amplification and space amplificaiton for blob 
file


Need to update mlsm_plot_metrics so that it accepts the single file from command
line as input instead of traversing all folders holding data from multiple run 
of db_bench


Let's get the write rate tomorrow 


Need to set up periodic scheduler to get the db stats 
Let's learn how to use the PeriodicTaskScheduler

Know how to use PeriodicTaskScheduler now.
But I don't know where is the db stats stored and how can I get the write rate.

db_impl.cc
void DBImpl::DumpStats()
I can modify GetProperty() function to get the write rate and store it  in DBImpl class
So now I know how to get interval write rate , I can just modify the DumpStats() function ? 
DBImpl::DumpStats() is registered to periodic_task_functions 
```
  periodic_task_functions_.emplace(PeriodicTaskType::kDumpStats,
                                   [this]() { this->DumpStats(); });
```
It really takes time to figure out how to get write rate data
This can help me get other interval db data if  I want more.
18

Need to change how compactiontrace reader read payload


Updating tracer to write write rate data for each trace record .

After this feature is obtained. I will first test the prediction or classification accuracy.
And then I will integrate the model into rocksdb .

Let's get the whole system running. 

Let's assume that we already know what's the perfect classfication for lifetime distribution. 

Machine leanring model choice
1. LightGBM
2. Q-learning ( reinforcment learning )


Read papers about meachine learing for storage system or machine learning for system
if we want to geta sense of broad view in this area.


Finally get the periodic num writes to the trace file. 
So now let's run python model training to do some validataion. 


Features
Used
1. key id ( trasform  from string to int, so basically useless right now.)
2. interval writes

To be used
3. adjacent keys or precusor keys write rate .(but how to get this one?)
4. 



Load model in c api in lightgbm
```
To load a LightGBM model in the C API, you can use the function `LGBM_BoosterCreateFromModelfile`. Here is an example of how to use it:

```c
#include <LightGBM/c_api.h>

int num_iterations;
BoosterHandle booster;
LGBM_BoosterCreateFromModelfile("model.txt", &num_iterations, &booster);


```
In this code, `"model.txt"` is the filename of your model. The function will load the model from this file into the `booster` variable. The `num_iterations` variable will hold the number of iterations that were used to train the model¹.

Please replace `"model.txt"` with the path to your actual model file. After calling this function, you can use the `booster` handle to make predictions¹. Remember to check the return value of `LGBM_BoosterCreateFromModelfile` to ensure that the model was loaded successfully¹.

Source: Conversation with Bing, 9/28/2023
(1) C API — LightGBM 4.1.0.99 documentation - Read the Docs. https://lightgbm.readthedocs.io/en/latest/C-API.html.
(2) lgbmclassifier save and load model · Issue #1217 · microsoft/LightGBM. https://github.com/microsoft/LightGBM/issues/1217.
(3) Load LightGBM model from string or buffer - Stack Overflow. https://stackoverflow.com/questions/55641285/load-lightgbm-model-from-string-or-buffer.
(4) Loading a model from Python into C++ just for predictions. #2397 - GitHub. https://github.com/microsoft/LightGBM/issues/2397.
(5) undefined. https://github.com/dmlc/treelite.
(6) undefined. https://lightgbm.readthedocs.io/en/latest/Parameters.html.


Make predictions in lightgbm with c api
```
To perform prediction with the LightGBM C API, you can use the `LGBM_BoosterPredictForMat` function¹. Here is an example of how to use it:

```c
#include <LightGBM/c_api.h>

BoosterHandle booster;
// Load model
LGBM_BoosterCreateFromModelfile("model.txt", &num_iterations, &booster);

// Assume float_data contains your data and num_row and num_col are the number of rows and columns
float *float_data = ...;
int64_t num_row = ...;
int32_t num_col = ...;

double *out_result = (double*)malloc(sizeof(double) * num_row);
int64_t out_len = 0;

// Perform prediction
LGBM_BoosterPredictForMat(booster, float_data, C_API_DTYPE_FLOAT32, num_row, num_col,
                          C_API_PREDICT_NORMAL, 0, -1, "", &out_len, out_result);

```
In this code, `float_data` is a pointer to your data stored in row-major order¹. `num_row` and `num_col` are the number of rows and columns in your data¹. `out_result` is a pointer to a buffer where the predictions will be stored¹. After calling `LGBM_BoosterPredictForMat`, `out_result` will contain the predicted values¹.

Please replace `"model.txt"` with the path to your actual model file and ensure that `float_data` points to your actual data¹.

Remember to check the return value of `LGBM_BoosterPredictForMat` to ensure that the prediction was successful¹. Also, don't forget to free the memory allocated for `out_result` when you're done with it¹.

Source: Conversation with Bing, 9/28/2023
(1) C API — LightGBM 4.1.0.99 documentation - Read the Docs. https://lightgbm.readthedocs.io/en/latest/C-API.html.
(2) How to use C_API(with category feature) to predict the data from stdin .... https://github.com/microsoft/LightGBM/issues/3334.
(3) LGBM inference using C++ API (Python/C++ Interface) - GitHub. https://github.com/PathofData/LGBM_C_inference.
(4) [C_API] LGBM model returns different predictions on Python and C++. https://github.com/microsoft/LightGBM/issues/5822.



Experiments log: with period_num_writes , features [key_id, insert_timestamp, period_num_writes]
```

See the caveats in the documentation: https://pandas.pydata.org/pandas-docs/stable/user_guide/indexing.html#returning-a-view-versus-a-copy
  features['period_num_writes'] = pd.to_numeric(features['period_num_writes'])
std_dev:  49876961.48668319
Starting training...
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.019648 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 743
[LightGBM] [Info] Number of data points in the train set: 348330, number of used features: 3
[LightGBM] [Info] Start training from score 62074424.213011
Starting predicting...
The rmse of prediction is: 46839367.17308591

```
Experiments log: without extra features, features[key_id, insert_timestamp]
```
/home/zt/rocksdb_kv_sep/mlsm_model/lightgbm_lifetime_prediction.py:21: SettingWithCopyWarning:
A value is trying to be set on a copy of a slice from a DataFrame.
Try using .loc[row_indexer,col_indexer] = value instead

See the caveats in the documentation: https://pandas.pydata.org/pandas-docs/stable/user_guide/indexing.html#returning-a-view-versus-a-copy
  features['key'] = pd.to_numeric(features['key'])
/home/zt/rocksdb_kv_sep/mlsm_model/lightgbm_lifetime_prediction.py:22: SettingWithCopyWarning:
A value is trying to be set on a copy of a slice from a DataFrame.
Try using .loc[row_indexer,col_indexer] = value instead

See the caveats in the documentation: https://pandas.pydata.org/pandas-docs/stable/user_guide/indexing.html#returning-a-view-versus-a-copy
  features['insert_time'] = pd.to_numeric(features['insert_time'])
std_dev:  49876961.48668319
Starting training...
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.082093 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 510
[LightGBM] [Info] Number of data points in the train set: 348330, number of used features: 2
[LightGBM] [Info] Start training from score 62074424.213011
Starting predicting...
The rmse of prediction is: 46848442.41926392
>>> exit()

```


Let's turn this job into classification task.
First let's inspect the valid duration and see what classifications we should give 
to this particular dataset.

I believe there is research paper about how to give automatic classifications 
for different workload.
I can try the simple binary classification task and integrate this model into rocksdb

Lightgbm classification usage example and evaluation of classification accuracy and recall. 
I forget the definition of recall rate and accuracy rate.
https://www.kaggle.com/code/prashant111/lightgbm-classifier-in-python#Classification-Metrices



There is too many parameters to tune.

Experiments with task changed to classification
age_cutoff: 1.0
gc_threshold: 0.8
long lifetime threashold: 0.5 * 1e8
Features:
[key_id, insert_time, period_write_rates]
Lightgbm params:
```
params = {
    'boosting_type': 'gbdt',
    'objective': 'binary',
    'metric': {'auc', 'binary_logloss'},
    # 'num_leaves': 31,
    'learning_rate': 0.05,
    'feature_fraction': 0.9
}

# Train the model
print('Starting training...')
gbm = lgb.train(params,
                lgb_train,
                num_boost_round=100,
                valid_sets=lgb_eval,
                )


```
Classification results:
```
std_dev:  0.49999998955478836
Starting training...
[LightGBM] [Info] Number of positive: 174252, number of negative: 174078
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.083323 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 743
[LightGBM] [Info] Number of data points in the train set: 348330, number of used features: 3
[LightGBM] [Info] [binary:BoostFromScore]: pavg=0.500250 -> initscore=0.000999
[LightGBM] [Info] Start training from score 0.000999
Starting predicting...
              precision    recall  f1-score   support

           0       0.65      0.48      0.56     43584
           1       0.59      0.74      0.66     43499

    accuracy                           0.61     87083
   macro avg       0.62      0.61      0.61     87083
weighted avg       0.62      0.61      0.61     87083
```


Let's try auto tune framework to get the best paratmeter to train the model
And then I will integrate this model into rocksdb 


Removing period_write_rates features
```
std_dev:  0.49999998955478836
Starting training...
[LightGBM] [Info] Number of positive: 174252, number of negative: 174078
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.084810 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 510
[LightGBM] [Info] Number of data points in the train set: 348330, number of used features: 2
[LightGBM] [Info] [binary:BoostFromScore]: pavg=0.500250 -> initscore=0.000999
[LightGBM] [Info] Start training from score 0.000999
Starting predicting...
              precision    recall  f1-score   support

           0       0.65      0.48      0.56     43584
           1       0.59      0.74      0.66     43499

    accuracy                           0.61     87083
   macro avg       0.62      0.61      0.61     87083
weighted avg       0.62      0.61      0.61     87083
```
Looks like adding period_num_writes features does not help at all


FEatures: [key_id, period_num_writes]
removing timestamp gives worse results
```
std_dev:  0.49999998955478836
Starting training...
[LightGBM] [Info] Number of positive: 174252, number of negative: 174078
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.083045 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 488
[LightGBM] [Info] Number of data points in the train set: 348330, number of used features: 2
[LightGBM] [Info] [binary:BoostFromScore]: pavg=0.500250 -> initscore=0.000999
[LightGBM] [Info] Start training from score 0.000999
Starting predicting...
              precision    recall  f1-score   support

           0       0.60      0.56      0.58     43584
           1       0.59      0.63      0.61     43499

    accuracy                           0.60     87083
   macro avg       0.60      0.60      0.60     87083
weighted avg       0.60      0.60      0.60     87083
```

Possible new features: 
1. num of compaction going on
2. read rates
3. precursor write rates
4. num of flushes recently
5.  turn string key into key ranges ( need to show that keys falling in same range have similar lifetime, 
is this assumption  true?) Anyway, it's worth trying

If we can get more application level information, then it would be better


Let's turn key into key ranges now


accuracy improve 9% after adding key_range_idx features.
This is great result  for now
```
See the caveats in the documentation: https://pandas.pydata.org/pandas-docs/stable/user_guide/indexing.html#returning-a-view-versus-a-copy
  features['key_range_idx'] = pd.to_numeric(features['key_range_idx'])
std_dev:  0.49999998955478836
Starting training...
[LightGBM] [Info] Number of positive: 174252, number of negative: 174078
[LightGBM] [Warning] Auto-choosing col-wise multi-threading, the overhead of testing was 0.085617 seconds.
You can set `force_col_wise=true` to remove the overhead.
[LightGBM] [Info] Total Bins 844
[LightGBM] [Info] Number of data points in the train set: 348330, number of used features: 4
[LightGBM] [Info] [binary:BoostFromScore]: pavg=0.500250 -> initscore=0.000999
[LightGBM] [Info] Start training from score 0.000999
Starting predicting...
              precision    recall  f1-score   support

           0       0.76      0.54      0.63     43584
           1       0.64      0.83      0.73     43499

    accuracy                           0.69     87083
   macro avg       0.70      0.69      0.68     87083
weighted avg       0.70      0.69      0.68     87083
```

What data should I use to test the db performance end to end?.


1. Working on use flaml parameter tunning framework to find best paramters to train model. [done]

2. Next I will integrate the model in torocksdb 
But what data should I use to test the ene-to-end performance?  
    I can use 
    1. all data  
    2. test data

3. space amplification   [done]


The feature importance fig generated by FLAML shows that insert time and key_range_idx
is the top 2 most important features.


Labels changed to exponential increasing duration 
Learn from Linnos that we could label the lifetime with exponential increasing duration
liek this 
1s, 10s, 100s, 1000s, 10000s and long lived.
I could try to plot the lifetime distribution as well.


In the meantime I need to integarte the model into rocksdb as well.

Lightgbm c API  import. -> Need to modify CMakelists.txt to import library as thir-party dependency


There is a test failed in lightgbm cpp unit tests. I can fix this if I have some time

difference between include_directories and add_subdirectory in cmake 
In CMake, `include_directories` and `add_subdirectory` serve different purposes¹:

- `include_directories(dir)`: This command is used to add the given directories to those the compiler uses to search for include files. These directories are added to the directory property `INCLUDE_DIRECTORIES` for the current CMakeLists file¹. It's used for adding headers search paths (`-I flag`)¹.

- `add_subdirectory(source_dir)`: This command is used to add a subdirectory to the build. There is also a CMakeLists.txt file in the source_dir. This CMakeLists.txt file in the specified source directory will be processed immediately by CMake before processing in the current input file continues beyond this command¹. It's used when you want to include another directory that contains a CMakeLists.txt file³⁴.

In summary, `include_directories` is used to specify directories where the compiler can find additional header files, while `add_subdirectory` is used to add a subdirectory that contains a CMakeLists.txt file to the build¹.

Source: Conversation with Bing, 10/6/2023
(1) CMake Difference between include_directories and add_subdirectory?. https://stackoverflow.com/questions/12761748/cmake-difference-between-include-directories-and-add-subdirectory.
(2) [CMake] Difference between ADD_SUBDIRECTORY and INCLUDE. https://cmake.org/pipermail/cmake/2007-November/017896.html.
(3) [CMake] Difference between ADD_SUBDIRECTORY and INCLUDE - narkive. https://cmake.cmake.narkive.com/afXctZak/difference-between-add-subdirectory-and-include.
(4) cmake: add_subdirectory() vs include() - Stack Overflow. https://stackoverflow.com/questions/48509911/cmake-add-subdirectory-vs-include.


Change classification labels into 4 with the exponential increasing duration
[1, 10, 100, 100]
Here's the results

Most of the lifetime of keys GCed in compaction falls in the range 10-100s
Maybe we need to get fine grained labels in this interval. But how ? 

Starting predicting...
              precision    recall  f1-score   support

           0       0.50      0.00      0.00      1165
           1       0.68      0.17      0.27     10007
           2       0.69      0.94      0.79     57050
           3       0.49      0.16      0.24     18678

    accuracy                           0.67     86900
   macro avg       0.59      0.32      0.33     86900
weighted avg       0.64      0.67      0.60     86900

precision score: 0.5873981957463573
The model is bad at predicting lifetime labels for short and long lifetime keys.
How can we improve this ? 
The only way I can come up right now is to use more features and useful features.

Tried more classification labels but get really bad prediction results
[1,2,4,8,16,32,64,128,256,512,1024]
              precision    recall  f1-score   support

           1       0.00      0.00      0.00      1165
           2       0.24      0.01      0.02      1222
           3       0.32      0.00      0.01      2286
           4       0.17      0.01      0.01      4420
           5       0.16      0.02      0.03      8037
           6       0.11      0.02      0.03     13631
           7       0.14      0.03      0.06     21215
           8       0.14      0.10      0.12     24569
           9       0.12      0.54      0.20     10355
          10       0.00      0.00      0.00         0

    accuracy                           0.11     86900
   macro avg       0.14      0.07      0.05     86900
weighted avg       0.14      0.11      0.08     86900



Need to do classification for all keys in the trace data instead of compaction trace 
data.

Number of compaction trace records: 434500
Number of write op trace records: 2500000


At the time of the end of db_bench running  
21% of 3 lifetime keys are GCed
67% of 2 lifetime keys are GCed
Nearly 100% of 1 and 0 lifetime keys are GCed
```

➜  with_gc_1.0_0.8 cut -d ' ' -f 9,9 ./op_valid_duration.txt | sort | uniq -c
   6209 0
  54652 1
 422608 2
 435988 3
      1 lifetime_exp_increase_label
➜  with_gc_1.0_0.8 cut -d ' ' -f 11,11 ./internal_key_lifetime.txt | sort | uniq -c
   6022 0
  50418 1
 284356 2
  93703 3
      1 lifetime_exp_increase_label

```
How can we get the level distribution for these keys? Because those
keys in compaction trace are discarded.


exponential increasing lifetime classificaiton task on op_keys  
The overall results are worse than that in the compaction trace data
```
              precision    recall  f1-score   support

           0       0.38      0.00      0.00      1315
           1       0.60      0.04      0.07     10937
           2       0.65      0.46      0.54     84094
           3       0.59      0.84      0.69     87546

    accuracy                           0.61    183892
   macro avg       0.55      0.33      0.32    183892
weighted avg       0.62      0.61      0.58    183892

0.5532965833971224
```

Binary classification task results on same op_keys data
The recall rate is low . So this means that many short lifetime keys 
are predicted to be long.
The cause might be sample imbalance because there are too many long lifetime
samples.
Regarding the  number of samples, let's check the figure plotted 
```
              precision    recall  f1-score   support

           0       0.74      0.22      0.34     54656
           1       0.75      0.97      0.84    129236

    accuracy                           0.75    183892
   macro avg       0.74      0.59      0.59    183892
weighted avg       0.74      0.75      0.69    183892
```

cpp example of how to use lightgbm to do model loading and prediction
https://github.com/PathofData/LGBM_C_inference/tree/master

Can not build lightgbm lib via Cmake file in rocksdb . So I compile lightgbm 
manually and install it in the system path.
include path: /usr/local/include/LightGBM
bin path: /usr/local/bin/lightgbm
lib path: /usr/local/lib/lib_lightgbm.a

I added Findlightgbm.cmake file so that header files and lib files of lightgbm
can be found when building rocksdb with CMakeLists.txt
But I still failed to build the executables and the errors are like this
```

```
It turns out that I build the lightgbm static lib with openmp lib function
but there is no openmp lib installed in the system. 
So I will rebuild the lightgbm lib and without using openmp functions .

Great, I build the db_bench successfully after I rebuild lightgbm excluding 
dependecy on openmp.

So now what I need to do is to add new datastructures in version? to store 
value blob with different lifetime labels

I think the best time to do predictions for keys is during the flush operation.
Or we could do predictions in a new thread so as not to degrade the flush performance
or qps or write rate.
Let's just go with first design now.
I am excited about the results. Hope we can get a positive results.

Then it comes to the garbage collection process
It could happen that a lot of SST files are included in the GC for 
one blob files if we take lifetime as the key factor to store values.

Let's try it now.
We can come up with solution later to solve this problem

I put the PredictLifetime function in DBImpl. I don't know if Flush function can 
call function of DBImpl or Flush function is part of the DBImpl class.

[Important] Another feature I can use is the key size and value_size . I didn't put key size as a feature before.
But some workloads have same key size.
I guess we might need to make assumptions. The key of the paper for now is to prove our idea can work. 
Features engineering is important

How can I get the key_range_id ?
Leaper get key range offline. So I guess we could use the same method.
We can come up with other methods to get key ranges later. Maybe use sampling.

Another prolem is that the key in op trace file is in hex format, maybe I need to
conversion so that hex format keys are turn back into string.

Or maybe I need to change trace analyzer code to write down string.

Was working on writing string of trace keys into trace file but now I realize
that I don't need to do that.  The reason I want to do this is because I want 
to get key range id for each key. Keys generated offline is in hex format.
But now I can load keys in key range list from file and then do conversion using 
HexToString in rocksdb.

Flush callstack:

[Idea] Use LLM to decide adjacent keys group.
[Idea] Update model for each compaction ? This idea does not necessaryily is meant 
to use for GC but can be use to schedule compaction.

immutable memtable is in ColumnFamilyData*cfd
FlushMemTable()
    MaybeScheduleFlushOrCompaction()
        BGWorkFlush()
            BackgroundCallFlush()
                DBImpl::BackgroundFlush()
                    DBImpl::FlushMemTablesToOutputFiles()
                        DBImpl::FlushMemTableToOutputFile()
                            FlushJob::Run()
                                FlushJob::WriteLevel0Table() at flush_job.cc
                                    BuildTable(std::vecotr<BlobFileAddition> blob_file_additions) at builder.cc
                                        std::unique_ptr<BlobFileBuilder> blob_file_builder(blob_file_additions)
                                        BlockBasedTableBuilder::Add(key, value) at block_based_table_builder.cc
                                            BlobFileBuilder::OpenBlobFileIfNeeded()
                                            WriteBlobToFile(key, blob, &blob_file_number, &blob_offset);
                                            const Status s = CloseBlobFileIfNeeded();
                                        CompactionIterator::PrepareOutput()
                                            CompactionIterator::ExtractLargeValueIfNeeded()
                                                CompactionIterator::ExtractLargeValueIfNeededImpl()
                                                    BlobFileBuilder::Add(key, value_, & blob_index_)
                                            GarbageCollectBlobIfNeeded();
                                    edit_->SetBlobFileAdditions()
                                cfd_->imm()->TryInstallMemtableFlushResults()
                                    // each memtable has a associated VersionEdit
                                    vset->LogAndApply(cfd, options, edit_list, mu, db_directory)
                                    VersionSet::LogAndApply()
                                        VersionSet::LogAndApply()  
                                            VersionSet::ProcessManifestWrites()
                            InstallSuperVersionAndScheduleWork()


Version {
    VersionStorageInfo {
        AddBlobFile()
        GetBlobfiles()
        Added: 
        std::vector<BlobFiles> lifetime_blob_files_;
          const BlobFiles& GetBlobFiles(uint64_t liftime_bucket) const {
            return lifetime_blob_files_[liftime_bucket];
          }
        void AddBlobFileWithLifetimeBucket(std::shared_ptr<BlobFileMetaData> blob_file_meta,
                                     uint32_t bucket_id);



    }
}


VersionEdit {
 
}
blob file SST file info is stored in VersionStorageInfo

I can add a setBoosterHandle in FlushJob to pass the reference to model to it .

How are current value blob files stored? This affects  the way I change current code


[Idea] It would be better if the model can update itself online so that it can learn the workload 
dynamically.

But right now let's use the model trained offline for our taks for simplicity.

Looks like value is generated by compaction iterator. I need to check the code to locate
where the value is put into blob file.

[Idea] Do prediction for keys during compaction as well besides Flush. So each lifetime of keys
can be adjusted dynamically. Or maybe we don't need model prediction. We can use some 
algorithm to decide where to put the key that is mispredicted.

[Idea] We could put blob files that are predicted to be invalidated at the same 
time together. Short lifetime blob files and long lifetime blob files maybe invalidated
at the same time because they are created in different time.


[Implementation] Just realize that I need to modify the read path code to read the 
value of blob files because we change how blob files are stored and indexed.

[Impl] Need to know how new version is built and update the code for adding blob files in new version
accordingly.

But right now let's just load the model and do a initial prediction . Part of the ene2end impl. 
How can I pass key context to model when doing predicion?
BuildTable() does not belong to DBImpl
Maybe I need to change iterator of memtable. Store context information in memtable and 
return context info with iterator. Or I can just return a vector 


One thing I realize I have not done yet is that I have not written the context 
storing code. Where should I store this part of information? 
[Impl ] option allow_concurrent_memtable_write is set to true right now. Set to false in
future impl


WriteBatchInternal::InsertInto() at write_batch.cc
    // this is not good. It reqruies extra work to get key features 
    // in WriteBatchInternal
    w->batch->Iterator(&inserter)
        MemTableInserter::PutCF()
            MemTableInserter::PutCFImpl()


It's a little bit challenging to store key features context along with each key
because key and values are stored in skip list and keys will be reordered during insertion.
We could do this . 
Set a map and put key features with key into map before inserting into skiplist.

[Impl] Change time format to hour:minute:second

I could get key features of key whilte iterating batch content 
during memtable insertion.

Batch content is traversed in writebatchinternal . JUst realize that db pointer is
passed to WriteBatchInternal::InsertInto()
So I guess we can all function of DB pointer to get key features.

[Impl] Allocate key range memory using  mem_tracker?
Let's not deal with this memory allocation for now.
I will just call new
I think I don't need to create KeyFeatures at WriteBatchInternal::Iterate()

[Impl]I can call arena_->Allocate() to allocate KeyFeatures memory.

Need to debug std::lower_bound in GetKeyFeatures()

Do predictions for keys during Flush job:
How can I get label for keys that are returned by iterator?
I can extract sequence number from the slice returned by MergingIterator 
I can pass DBImpl to CompactionIterator as well.

Can I be faster when I changed the code to load model to do predictions?
I can load the model directly in FlushJob. But that would costly because
I need to load model each time a FlushJob is scheduled. 

It takes quite a while to finish the first working version of my DumpLSM
implementation.


lightgbm fastsinglerow prediction function introduction.
The PR says that this function is used to high throughput situation.
https://bleepcoder.com/lightgbm/585801627/performance-single-row-predictions-speedup

The small model and fast model should have its own marketplace in this AI era.


[Idea] I can do a machine leanring framework summary to see what's the current
situation for small high thoughtput model.


Finished load model and pass the model to compaction iterator. 
Finished doing prediction for keys during flush  job.
[Todo] Test predictions of model will actually work. Correctness

~~[Todo] Need to add different lifetime buckets for BlobFileBuilder~~

I can just create multiple BlobFileBuilder for FlushJob instead of
creating multiple BlobFileWriter inside BolbFileBuilder.

[Todo]I may need to do a cpp model loading and performance testing to show that 
my cpp code works like python code to avoid some coding bugs during 
inference..


So now it comes to the part where I should update versionEdit and InstallVersion()
So that we can do GC later smoothly.

So here's what I think the current GC in rocksdb do. 
Obsolete keys are dropped during normal compaction and values of these
dropped keys are recognized as garbage in the corresponding blob files.
So garbage in each blob files  gradually increase and finally GC is 
triggered once garbage exceeds threshold.
Limitation: current implementation chooses the oldest blob files 
as the starting point and assumes that older blob files have more garbage 
than younger blob files.
Where is  the code for updating garbage num in blob files ?
compaction_outputs.cc
```
Status CompactionOutputs::AddToOutput(
    const CompactionIterator& c_iter,
    const CompactionFileOpenFunc& open_file_func,
    const CompactionFileCloseFunc& close_file_func) {

    s = blob_garbage_meter_->ProcessOutFlow(key, value);
```

Now I face the chicken-egg problem. I overwrite the previous data I get from 
native rocksdb so I don't have data to get key ranges to do prediction right now.

git command to fetch submoduel recursively. I always forget this command
```
git submodule update --init --recursive
```

[Idea + Impl]  Put key features in memtables into vector so that one prediction
call needs to be made during Flush job.

db_bench run with faulre after flush several memtables.
Let's figure out why. Use gdb to do it.
There's segmentation fault. So I think it's memory access issue.
So the issue is compaction iterator is not set to have booster config 
when compaction is started. Because I only set booster handle for compaction
iterator in flush job.

Another bug
TryAgain status is returned by memtable add function
The msg is "key+seq" exists
Why does the skiplist return status not ok?
Found the bug, it's because I don't comment the PUtCF() function
after PutCFWithFeatures() so there's duplicate write 
to the memtable.

How do I deal with Delete ?
Delete is not written to blob files
Let the DeleteCFImpl call PutCFImpl. PutCFImpl puts delete to skiplist


Try to get detail of how new version is installed and how new blob files
are added to new version. 
The code is too long. Need some doc help

VersionSet::ProcessManifestWrites()
    VersionSet::LogAndApplyHelper(cfd, VersionBuilder, VersionEdit)
        VersionBuilder::Rep::Apply(VersionEdit*)
            ApplyBlobFileAddition(BlobFileAddition) at version_set.cc
                vs->AddObsolteBlobFile()
            MergeBlobfileMetas()


Is cfd in version or is cfd in versionset?
So now I know how to apply versionedit and put blob files labeled with different
lifetime into different bucket.


These are the ideas that I get from watching machine learning 
for cpp memory allocator talk from Martin Mass.
https://www.youtube.com/watch?v=4_UdAR_5jqk
[Idea] integrate model into system in a pluggable way.

[Idea] Easy online and office line model integration.

[Idea] Easy data collection and feature engineering. Can we use LLM for this?

[Idea] Genralization of machine learning model to system 


After fixing bugs of previous flush job code. Now I can start working 
on doing GC on rocksdb with lifetime information.


Get trace data from this link
http://iotta.snia.org/traces
This trace data site contains lots of data traces including 
block i/o trace, hpc summaries, key-value traces.
I knew this link from this paper: https://huaicheng.github.io/p/sosp21-ioda.pdf

Block I/O trace data 
http://iotta.snia.org/traces/block-io/388

Installing new version code is at here: version_set.cc:5275

version_set.cc:5010
```
    for (int i = 0; i < static_cast<int>(versions.size()); ++i) {
      assert(!builder_guards.empty() &&
             builder_guards.size() == versions.size());
      auto* builder = builder_guards[i]->version_builder();
      Status s = builder->SaveTo(versions[i]->storage_info());
      if (!s.ok()) {
        // free up the allocated memory
```
builder->SaveTo()
    VersionBuilder::SaveBlobFilesTo(vstorage)
        AddBlobFileIfNeeded()
            vstorage->AddBlobFile()
        MergeBlobfileMetas()


So how should I chaneg current blob file storage structure?
The goal is that values of keys that are invalidated at the same time 
should be put together. ?
But from what I see in the paper of learning memory allocation.

[Idea]Will move blob to long lifetime if current garbage collection does not
GC current value and key

[Idea] Use multiple models for system task?

[Idea] Embedding keys to get better adjacency.


[Idea] Test storage performance when there is multiple vm using it.
Can we get stable latency or schedule to get higher performance?
Or how can we get same amount of I/O with cheaper price. 

I can read papers from Socc to catch the latest progress in cloud 
computing and storage


[Idea] Disaggregated memory for machine learing

[ Idea]  LSM-tree storage on disaggregated memory I don't think 
this is worth researching though. Because memtable is small .
What if we put nvm and connect them by rdma ??


space amp is larger with gc


[Idea] get garbage percentage for each blob file in regular interval

UIUC AP points out that there is random access if 
we have multiple lifetime categories. 

What's difference between saveto() and apply()

ApplyBlobFileAddition() goes before SaveTo()

How can I get average garbage per blob ?  
Snapshot?


Update this  function to get correct behavior?
Current implementation travese lifetime mutable file metas. 
```

  void MergeBlobFileMetasWithLifetime(uint64_t first_blob_file, ProcessBase process_base,
                          ProcessMutable process_mutable,
                          ProcessBoth process_both,
                          uint64_t lifetime) const {

```

Make blob_files_ unused  and use lifetime_blob_files_ instead. 
Don't know how this will affect garbage collectino process.

So now our task it to update the garbage collection process
 code and  test the write amplification first.


Add alternaltive function for VersionStorageInfo::ComputeFilesMarkedForForcedBlobGC()
Current gc logic is that  


Need to think about how to add timestamp to the blob file when creating 
blob files duing comaction process that happens on higher levels other 
than flush.


I want to do a simple test now. So I will come up wih a simple 
implementation that can help me to get results of idea asap.

How about putting all the values back to the same lifetime bucket?
This brings another problem which is that k/v pair in the orignial 
blob files are not traversed sequentially during the compaction process.
The essential piece is that k/v pairs stored in previous blob files
consumes some of its lifetime when it's rewritten to new blob files.
So the remaining lifetime of k/v pairs need to be reducted from the original
estimated lifetime.

I can add a method to the iterator to return estimated remaining lifetime 
for each k/v. We assume that lifetime for each k/v pair in the blob file
are same.


Again, the problem is that compaction iterator traverse keys in the order 
that is not the same as that in blob/value file.

[Idea and implementation] I may need to add another implementation of 
garbage collection which reads all of values and put them into LSM-tree
again. If we use the original implentation of gc then we may read lots of 
values other than the values in the original blob files .

Have two implementation choices for gc.
1. Add lifetime interface to iterator 
2. Do put op to LSM-tree during gc process.


I will go with the first one because I think it's more easier.
Don't know what's the first version implementation of blobdb


Where is internal iterator first initiated?

compaction_job.cc
get iterator from versions.
It's much more work than I anticipate.
```
  // Although the v2 aggregator is what the level iterator(s) know about,
  // the AddTombstones calls will be propagated down to the v1 aggregator.
  std::unique_ptr<InternalIterator> raw_input(versions_->MakeInputIterator(
      read_options, sub_compact->compaction, range_del_agg.get(),
      file_options_for_read_, start, end));
  InternalIterator* input = raw_input.get();


```

values are added to new blob file in prepareoutput() function.
blob file number and pos index number are read in this function.
so we can get the lifetime of blob file in this function.
get the creation time of blob file in this function as well. 

compaction iterator does not have version storage instance.
Pass the version information to compaction iterator?


flush_job.cc
get vstorage from column family data
```
  auto vstorage = cfd_->current()->storage_info();
  for (int level = 0; level < vstorage->num_levels(); ++level) {
    stream << vstorage->NumLevelFiles(level);
  }
  stream.EndArray();



```

[Impl] Need to add a map to vstorage to map blob file number to its blobfilemetadata?


[Impl notice] Need to set lifetime bucket number.


[Todo] Need to consider the situation that memtable is not filled full 
after long time. There is long time elapse between the key inserted and 
the key flushed. May need to deduct this period of time on which keys spent
on memtable.
For, we simply assume that memtable will be filled full in very short time 
compared to lifetime of keys. 

[Todo] set creation timestamp when blobfilebuilder is initiated or pick the timestamp
of key that is oldest among all keys?
I will set timestamp of creation time of blobfilebuilder as creation timestamp
for blob file for now. 


I think I officially finish the first version of dumplsm implementation which is super exciting 
and good.

So now let's try to do test and test the performance


[Todo] Need to record the trace data and then train the model and test the results.

Test plan
1. Try binary classification model
2. Try multi classificaiton model
Then change garbage collection logic so that only values in blob files that are
garbage collected will be put rewritten back to LSM-tree instead of doing compaction.
However, this approach has its own problem. keys associated with values might still be 
in LSM-tree and we might need to do get to see if value is still valid?
How does wisckey deal with this? Wisckey needs to do get to check the validity of 
values in blob files as well.

oh my god, we are not finished yet. Need to finish the computegarbagescore() to 
pick the blob files to be gced.



Will call trace replay to replay the trace so that we don't need to implement
the logics to read trace file and put keys back to db.

Far more work than I anticipated need to be done to get the test results..

each column family data has its own lsm-tree
https://cf2.cloudferro.com:8080/swift/v1/AUTH_5e376cddf8a94f9294259b5f48d7b2cd/ceph/rocksdb_in_ceph.pdf


[ Error to be solved] handle is nullptr when doing replay
check Open(&open_options_)
Looks like db_cfh is not initialized 
solution, initialize cfh of db in Open() function
[Solved]

[Todo] submit pr to fix db_bench_tool when running with replay
[Status: Not started]

[Error to be solved] initialize lifetime_blob_files_ vector to have lifetime bucket num size
[Fixed]

[Bug] SharedMeta intialized to be nullptr and is referenced in mergemeta
The problem is that shared_meta is moved to mutable_blob_file_metas vector
so it is moved to lifetime_mutable_blob_file_metas after thato.
So only nullptr is stored in lifetime_mutable_blob_file_metas.
SharedBlobFileMetaData is created with deleter.
So what about MutableBlobFileMetaData ?

Looks like there is no destructor function in MutableBlobFileMetaData.
reference count of shared_ptr in class will be automatically called
when class is destructed.

MutableBlobFileMetaData can only be created from pre-existing blob files
or new blob files which are represented by different data structures. 
Modify the constructor of MutableBlobFileMetaData so that it does not 
do std::move(SharedBlobFileMetaData).
So now SharedBlobFileMetaData is shared between mutable_blob_file_metas and 
lifetime_mutable_blob_file_metas.
[Fixed]

[Todo] Need to add BlobFileMetaData to blob_files_ and lifetime_blob_files_ at the same
time so that other functions can work as the same as before.
[Done]


[Bug] replyed failed. 
CheckConsistency(VersionStorageInfo*)
root cause: didn't add blobfilemetadata to unordered_map I added before.
add blobfilemetadata to unordered_map, doesn't work
Update AddBlobFileAddtion in VersionEdit to fix this ?
What causes this problem?
Maybe the reason is that I don't pass vector of 
Still haven't fixed this bug

LinkSst() is called in ApplyFileAddition()
Need to check this function
```
  Status ApplyFileAddition(int level, const FileMetaData& meta) {
```

call stack
```
#0  rocksdb::VersionBuilder::Rep::CheckConsistencyDetails (this=0x1555500043c0,
    vstorage=0x155550018680)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:529
#1  0x0000555555899338 in rocksdb::VersionBuilder::Rep::CheckConsistency (
    this=0x1555500043c0, vstorage=0x155550018680)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:551
#2  0x000055555589ca8e in rocksdb::VersionBuilder::Rep::SaveTo (this=0x1555500043c0,
    vstorage=0x155550018680)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:1432
#3  0x0000555555894cff in rocksdb::VersionBuilder::SaveTo (this=0x155550004060,
    vstorage=0x155550018680)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:1560
#4  0x00005555558d0ad7 in rocksdb::VersionSet::ProcessManifestWrites (
    this=0x5555568bb8b0, writers=std::deque with 1 element = {...}, mu=0x5555568bffc0,
    dir_contains_current_file=0x5555568d7560, new_descriptor_log=false,
    new_cf_options=0x0)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_set.cc:5114
#5  0x00005555558d4515 in rocksdb::VersionSet::LogAndApply(rocksdb::autovector<rocksdb::ColumnFamilyData*, 8ul> const&, rocksdb::autovector<rocksdb::MutableCFOptions const*, 8ul> co
nst&, rocksdb::autovector<rocksdb::autovector<rocksdb::VersionEdit*, 8ul>, 8ul> const&, rocksdb::InstrumentedMutex*, rocksdb::FSDirectory*, bool, rocksdb::ColumnFamilyOptions const*
, std::vector<std::function<void (rocksdb::Status const&)>, std::allocator<std::function<void (rocksdb::Status const&)> > > const&) (this=0x5555568bb8b0,
    column_family_datas=..., mutable_cf_options_list=..., edit_lists=...,
    mu=0x5555568bffc0, dir_contains_current_file=0x5555568d7560,
    new_descriptor_log=false, new_cf_options=0x0,
    manifest_wcbs=std::vector of length 0, capacity 0)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_set.cc:5626
#6  0x00005555556ca49b in rocksdb::VersionSet::LogAndApply (this=0x5555568bb8b0,
    column_family_data=0x5555568e1a80, mutable_cf_options=..., edit=0x155550001d08,
    mu=0x5555568bffc0, dir_contains_current_file=0x5555568d7560,
    new_descriptor_log=false, column_family_options=0x0)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_set.h:1221
#7  0x0000555555d2ed27 in rocksdb::CompactionJob::InstallCompactionResults (
    this=0x155554f42710, mutable_cf_options=...)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/compaction/compaction_job.cc:1717
#8  0x0000555555d29b9e in rocksdb::CompactionJob::Install (this=0x155554f42710,
    mutable_cf_options=...)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/compaction/compaction_job.cc:829
#9  0x000055555574bffd in rocksdb::DBImpl::BackgroundCompaction (this=0x5555568bf7c0,
    made_progress=0x155554f42d3f, job_context=0x155554f42da0, log_buffer=0x155554f42fa0,
    prepicked_compaction=0x0, thread_pri=rocksdb::Env::LOW)
```
When is oldest_blob_filenumber updated for each FileMetaData.
Don't find out the root cause.
The problem happens at compaction.
Maybe the code I write to put blob file does't work.

There is blob file meta in lifetime_blob_files but not in blob_files.
There is blob file addition in version edit in flush job.
there is SharedBlobFileMetaData in builder->rep_->mutable_blob_file_metas
but not in lifetime_mutable_blob_file_metas
gdb info
```
(gdb) p builder->rep_->mutable_blob_file_metas_
$31 = std::map with 1 element = {[10] = {shared_meta_ = warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'
warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'

std::shared_ptr<rocksdb::SharedBlobFileMetaData> (use count 3, weak count 0) = {
      get() = 0x1555480024d0}, delta_ = {additional_garbage_count_ = 0,
      additional_garbage_bytes_ = 0, newly_linked_ssts_ = std::unordered_set with 1 element = {                                                                                                           [0] = 9}, newly_unlinked_ssts_ = std::unordered_set with 0 elements},
    linked_ssts_ = std::unordered_set with 1 element = {[0] = 9}, garbage_blob_count_ = 0,
    garbage_blob_bytes_ = 0}}
(gdb) p builder->rep_->lifetime_mutable_blob_file_metas_[0]
$32 = std::map with 1 element = {[10] = {shared_meta_ = warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'
warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'

std::shared_ptr<rocksdb::SharedBlobFileMetaData> (use count 3, weak count 0) = {
      get() = 0x1555480024d0}, delta_ = {additional_garbage_count_ = 0,
      additional_garbage_bytes_ = 0, newly_linked_ssts_ = std::unordered_set with 0 elements,
      newly_unlinked_ssts_ = std::unordered_set with 0 elements},
    linked_ssts_ = std::unordered_set with 0 elements, garbage_blob_count_ = 0,
    garbage_blob_bytes_ = 0}}
```


make sure sharedblobfilemetadata is the same between blob_files_ and lifetime_blob_files_
```
(gdb) p *(versions[i]->storage_info()->lifetime_blob_files_[0][0])
$48 = {shared_meta_ = warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'
warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'

std::shared_ptr<rocksdb::SharedBlobFileMetaData> (use count 3, weak count 0) = {
    get() = 0x1555480024d0}, linked_ssts_ = std::unordered_set with 0 elements,
  garbage_blob_count_ = 0, garbage_blob_bytes_ = 0, create_timestamp_ = 17338454175503460425,
  lifetime_label_ = 2211685959070656598}
(gdb) p *(versions[i]->storage_info()->lifetime_blob_files_[0][0]).GetBlobFileNumber()
Couldn't find method std::shared_ptr<rocksdb::BlobFileMetaData>::GetBlobFileNumber
(gdb) p (versions[i]->storage_info()->lifetime_blob_files_[0][0]->GetBlobFileNumber())
$49 = 10
```

```
(gdb) p *versions[i]->storage_info()->blob_files_[0]
$40 = {shared_meta_ = warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'
warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'

std::shared_ptr<rocksdb::SharedBlobFileMetaData> (use count 3, weak count 0) = {
get() = 0x1555480024d0}, linked_ssts_ = std::unordered_set with 0 elements,

  garbage_blob_count_ = 0, garbage_blob_bytes_ = 0, create_timestamp_ = 17338454175503460425,
  lifetime_label_ = 2211685959070656598}
(gdb) p versions[i]->storage_info()->blob_files_[0]->GetBlobFileNumber()                                                                                                                          $41 = 10
```
blob file:10 is sotred in version storage before version builder calls SaveTo function.


check consistency is called before SaveTo() so that it make sure that 
version vefore SaveTo() is consistent

However, blob file:10 also shows up in edit_list[0]->blob_file_additions_;
which is supposed not to be here.

so vstorage should not have blob file:10 ?



```
#1  0x0000555555885f66 in rocksdb::VersionBuilder::Rep::CheckConsistency (this=0x1555480294c0,                                                                                           [45/1456]    vstorage=0x15554801d180)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:551
#2  0x00005555558896bc in rocksdb::VersionBuilder::Rep::SaveTo (this=0x1555480294c0,
    vstorage=0x15554801d180)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:1432
#3  0x000055555588192d in rocksdb::VersionBuilder::SaveTo (this=0x155548015980,
    vstorage=0x15554801d180)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_builder.cc:1560
#4  0x00005555558bd66f in rocksdb::VersionSet::ProcessManifestWrites (this=0x5555569f8950,
    writers=std::deque with 1 element = {...}, mu=0x555556a00840,
    dir_contains_current_file=0x555556a16670, new_descriptor_log=false, new_cf_options=0x0)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_set.cc:5116
#5  0x00005555558c10ad in rocksdb::VersionSet::LogAndApply(rocksdb::autovector<rocksdb::ColumnFamilyData*, 8ul> const&, rocksdb::autovector<rocksdb::MutableCFOptions const*, 8ul> const&, rocksdb::autovector<rocksdb::autovector<rocksdb::VersionEdit*, 8ul>, 8ul> const&, rocksdb::InstrumentedMutex*, rocksdb::FSDirectory*, bool, rocksdb::ColumnFamilyOptions const*, std::vector<std::function<void (rocksdb::Status const&)>, std::allocator<std::function<void (rocksdb::Status const&)> > > const&) (this=0x5555569f8950, column_family_datas=..., mutable_cf_options_list=...,
    edit_lists=..., mu=0x555556a00840, dir_contains_current_file=0x555556a16670,
    new_descriptor_log=false, new_cf_options=0x0,
    manifest_wcbs=std::vector of length 1, capacity 1 = {...})
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_set.cc:5628
#6  0x0000555555839e03 in rocksdb::VersionSet::LogAndApply(rocksdb::ColumnFamilyData*, rocksdb::MutableCFOptions const&, rocksdb::autovector<rocksdb::VersionEdit*, 8ul> const&, rocksdb::InstrumentedMutex*, rocksdb::FSDirectory*, bool, rocksdb::ColumnFamilyOptions const*, std::function<void (rocksdb::Status const&)> const&) (this=0x5555569f8950, column_family_data=0x555556a18a10,
    mutable_cf_options=..., edit_list=..., mu=0x555556a00840,
    dir_contains_current_file=0x555556a16670, new_descriptor_log=false,
    column_family_options=0x0, manifest_wcb=...)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/version_set.h:1240
#7  0x00005555558365ef in rocksdb::MemTableList::TryInstallMemtableFlushResults (
    this=0x555556a19318, cfd=0x555556a18a10, mutable_cf_options=..., mems=...,                                                                                                                        prep_tracker=0x555556a018b8, vset=0x5555569f8950, mu=0x555556a00840, file_number=9,
    to_delete=0x155554d41e88, db_directory=0x555556a16670, log_buffer=0x155554d41fc0,                                                                                                                 committed_flush_jobs_info=0x155554d41260, write_edits=true)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/memtable_list.cc:581
#8  0x00005555557db530 in rocksdb::FlushJob::Run (this=0x155554d40e10,
    prep_tracker=0x555556a018b8, file_meta=0x155554d408f0, switched_to_mempurge=0x155554d4085d)                                                                                                       at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/flush_job.cc:305
#9  0x0000555555724b91 in rocksdb::DBImpl::FlushMemTableToOutputFile (this=0x555556a00000,
    cfd=0x555556a18a10, mutable_cf_options=..., made_progress=0x155554d41d63,
    job_context=0x155554d41dc0, flush_reason=rocksdb::FlushReason::kWriteBufferFull,                                                                                                                  superversion_context=0x155548000ee0, snapshot_seqs=std::vector of length 0, capacity 0,
    earliest_write_conflict_snapshot=72057594037927935, snapshot_checker=0x0,
    log_buffer=0x155554d41fc0, thread_pri=rocksdb::Env::HIGH)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/db_impl/db_impl_compaction_flush.cc:271
#10 0x0000555555725583 in rocksdb::DBImpl::FlushMemTablesToOutputFiles (this=0x555556a00000,
    bg_flush_args=..., made_progress=0x155554d41d63, job_context=0x155554d41dc0,
    log_buffer=0x155554d41fc0, thread_pri=rocksdb::Env::HIGH)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/db_impl/db_impl_compaction_flush.cc:392
#11 0x0000555555734fa9 in rocksdb::DBImpl::BackgroundFlush (this=0x555556a00000,                                                                                                                      made_progress=0x155554d41d63, job_context=0x155554d41dc0, log_buffer=0x155554d41fc0,                                                                                                              reason=0x155554d41d64, thread_pri=rocksdb::Env::HIGH)                                                                                                                                             at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/db_impl/db_impl_compaction_flush.cc:2904                                                                                                  #12 0x000055555573563b in rocksdb::DBImpl::BackgroundCallFlush (this=0x555556a00000,                                                                                                                  thread_pri=rocksdb::Env::HIGH)
    at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/db_impl/db_impl_compaction_flush.cc:2949                                                                                                  #13 0x000055555573451e in rocksdb::DBImpl::BGWorkFlush (arg=0x15550a90bea0)                                                                                                                           at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/db_impl/db_impl_compaction_flush.cc:2758 
```
Will vstorage be updated when doing compaction ?

breakpoint 1: version_builder.cc ApplyBlobFileAddition()
breakpoint 2: version_builder.cc 

Fisrst Apply(), then Saveto()

Pay attention to 
ApplyFileAddition(int level, FileMetaData& meta)
which is called by Apply(VersionEdit* edit)
```
    const uint64_t blob_file_number = f->oldest_blob_file_number;

    if (blob_file_number != kInvalidBlobFileNumber) {
      MutableBlobFileMetaData* const mutable_meta =
          GetOrCreateMutableBlobFileMetaData(blob_file_number);
      if (mutable_meta) {
        mutable_meta->LinkSst(file_number);
      }
    }


```
Pay attention to AddBlobFileWithLifetimeBucket() in VersionStorageInfo()
Find out  that  mutable_blob_file_metas_ and lifetime_mutable_blob_file_metas_
does not share memory space so that linked_ssts_ is not the same in these
two struct in  VersionBuilder
How to fix this problem ?
1. make sure to call linksst() of both mutable_blob_file_metas_ and lifetime_mutable_blob_file_metas_
2. make MutableBlobFileMetaData as shared_ptr.

So now we pass the CheckConsistency()  function
[Fixed?]


[Todo] Put k/v to batch when doing gc that only gc values in the blob file 
that is supposed to be dead .

[Todo] Constructor of CompactionIterator()
[Done]

[Todo] Fast convert KeyFeatures to vector of double to do prediction
Proposal: Maybe store features as vector in mem add function.
So that we don't store struct .
[Status: Not started]



[Todo] Need to make sure order of features in vector of double align with 
that when doing training.
Feature order when doing training:
[Done]


Best practice to encode timestamp for training model
```
There are several best practices for encoding timestamp features for machine learning models:

1. **Discrete Components**: Break down the timestamp into discrete components such as year, quarter, month, day, and hour¹. This approach allows the model to capture patterns that occur at regular intervals.

2. **Time Elapsed Since an Event**: Another approach is to calculate the time elapsed since a particular event. This could be the Unix time (the number of seconds that have elapsed since the Unix epoch), or it could be a more domain-specific event¹.

3. **Cyclical Encoding**: For cyclical features like hours in a day or days in a week, you can use sin and cos functions to preserve the cyclical nature¹. This helps the model understand that certain times are close to each other, even though their numerical values might be far apart (e.g., 23:59 is close to 00:01).

4. **Neural Networks**: If your data has time-based cycles, you can pick a neural network that can capture those patterns. Long Short Term Memory (LSTM) is one of the most popular options¹.

5. **UTC Time**: Convert timestamp to UTC-time and discretize its values by some threshold (seconds, minutes, etc.)⁴.

Remember, the best approach depends on your specific use case and the nature of your data. It's often a good idea to experiment with different methods and see which one works best for your model.

Source: Conversation with Bing, 11/12/2023
(1) machine learning - Add timestamp as a feature to model - Data Science .... https://datascience.stackexchange.com/questions/107573/add-timestamp-as-a-feature-to-model.
(2) machine learning - Training the classfication model with the timestamp .... https://stackoverflow.com/questions/62589383/training-the-classfication-model-with-the-timestamp.
(3) Three Approaches to Encoding Time Information as Features for ML Models .... https://developer.nvidia.com/blog/three-approaches-to-encoding-time-information-as-features-for-ml-models/.
(4) Best practice for encoding datetime in machine learning. https://stats.stackexchange.com/questions/311494/best-practice-for-encoding-datetime-in-machine-learning.
```
I can try UTC  first
code example to convert timstamp to year/month/day/hour
```
import pandas as pd
from datetime import datetime
import pytz

# Assume df is your DataFrame and 'timestamp' is your timestamp column
df['timestamp'] = pd.to_datetime(df['timestamp'])

# Convert the timestamp to UTC
df['timestamp'] = df['timestamp'].dt.tz_convert(pytz.UTC)

df['year'] = df['timestamp'].dt.year
# Extract features from the timestamp
df['month'] = df['timestamp'].dt.month
df['day'] = df['timestamp'].dt.day
df['hour'] = df['timestamp'].dt.hour

```


Challenges 
1. feature selection
2. Model integration.
3. Deal with misprediction
4. [maybe] continuous model update ( This is a new problem)
5. Define lifetime classification



[Todo] Need to log classification of keys during flush job and compaction
[Status: Not started]


[Bug] no blob file is generated.
There is no blob file addtion in VersionEdit
Seems that I find the root cause .
ExtractLargeValueIfNeededImpl() return directly becuase blob_file_builder_ is nullptr
and no error is reported.
[Seems fixed]


[Todo] Get number of classification from lifetime_blob_builders. 
[Done]

[Bug] Segmentation fault calling LGBM_BoosterPredictForMatSingleRowFast()
But I can not get which variable is nullptr.
So now I will compile the lightgbm as debug mode adn try to debug again.
Assign fast_config_handle to fast_config_handle_ in compaction iterator.
[Fixed]

cmake command to build lightgbm with debug and static lib
```
cmake -DUSE_DEBUG=ON -DUSE_OPENMP=OFF -DBUILD_STATIC_LIB=ON ..
```


[Todo]Need to update  lifetime label to sec map.
[Fixed]


[Bug] There is only one blob file generated in first flush job which is
not what I expect. Let's fix the previous bug and keep watching.
[Not a bug]


Looks like oldest_blob_filenumber is set in sst filemeta in callbck function


Figure out version edit, version storage , and their relationship with compaction/flush job?
VersionEdit will be applied to VersionBuilder by calling ApplyBlobFileAddition()
And then in SaveTo() function VersionStorageInfo will be updated by VersionBuilder

[Bug] FastGetBlobFileMetaData() return not found for the blob file number.
Seems that it's not a bug , blob file:14 is newly generated.
[Not a bug]


[Bug] blob file [25-38] exist in the db folders but not in blob_files_ in VersionStorage
And db report that it can not find blob file 27 in blob_files_;

```
(gdb) p vstorage->blob_file_numer_to_blob_meta_map_
$25 = std::unordered_map with 2 elements = {
  [35] = std::shared_ptr<rocksdb::BlobFileMetaData> (use count 3, weak count 0) = {get() = 0x155548013d30},
  [25] = std::shared_ptr<rocksdb::BlobFileMetaData> (use count 3, weak count 0) = {get() = 0x155550015e80}}
```

```
(gdb) p *vstorage->blob_files_[0]
$28 = {shared_meta_ = warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'
warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'

std::shared_ptr<rocksdb::SharedBlobFileMetaData> (use count 1, weak count 0) = {get() = 0x15555001f1c0},
  linked_ssts_ = std::unordered_set with 1 element = {
    [0] = 26}, garbage_blob_count_ = 52156,
  garbage_blob_bytes_ = 44296397, create_timestamp_ = 0,
  lifetime_label_ = 0}
(gdb) p *vstorage->blob_files_[1]
$29 = {shared_meta_ = warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'
warning: RTTI symbol not found for class 'std::_Sp_counted_deleter<rocksdb::SharedBlobFileMetaData*, rocksdb::VersionBuilder::Rep::ApplyBlobFileAddition(rocksdb::BlobFileAddition const&)::{lambda(rocksdb::SharedBlobFileMetaData*)#1}, std::allocator<void>, (__gnu_cxx::_Lock_policy)2>'

std::shared_ptr<rocksdb::SharedBlobFileMetaData> (use count 1, weak count 0) = {get() = 0x155548016100},
  linked_ssts_ = std::unordered_set with 1 element = {
    [0] = 34}, garbage_blob_count_ = 0,
  garbage_blob_bytes_ = 0, create_timestamp_ = 0,
  lifetime_label_ = 0}
```
we have 25 and 35 blob file 


check consistency between blob_file_numer_to_blob_meta_map_ and blob_files_ and 
lifetime_blob_files_
They are consistent.

Looks like blob_files_ is built in SaveBlobFilesTo() function in VersionBuilder()

No blob file 27 generated log in LOG which is not right.
Need to check how blob files are recorded in log and then we might find out the
root cause and fix this.

Seems that no blob file log shows up in compaction whose compaction reason is 
"ForcedBlobGC"
Need to make sure garbage count is updated and newly generated blob files ared 
added to VersionStorage.

31, 29, 27 blob file is missing from the LOG .
Seems that I find the root cause of the bug. 
I don't write code that closes the blob file builders in compaction_job.cc

So now I have a clear idea of how VersionEdit, VersionBuilder, VersionStorageInfo work together.
VersionBuilder::Apply(VersionEdit *edit) will first apply versionedit such as blob file addition and file addition
to create mutable_blob_file_meta.
And then VersionBuilder::SaveTo() will save blobfilemetadatas to VersionStorageInfo
```
  Status SaveTo(VersionStorageInfo* vstorage) const {
```
Why is this?
I don't remember there is VersionBuilder in leveldb.

Difference between blobdb and  old stackable blobdb
```
The main differences between the new integrated BlobDB and the old Stackable BlobDB in RocksDB are as follows:

1. **API**: The old BlobDB had its own StackableDB-based interface (`rocksdb::blob_db::BlobDB`), while the new BlobDB can be used via the well-known `rocksdb::DB` API¹².

2. **Consistency**: With the integrated BlobDB implementation, RocksDB’s consistency guarantees and various write options (like using the WAL or synchronous writes) now apply to blobs as well¹.

3. **Write Performance**: The old BlobDB required synchronization and performed expensive operations like compression in the application thread. The new BlobDB offloads blob file building to RocksDB’s background jobs, i.e., flushes and compactions¹.

4. **Read Performance**: The new BlobDB improves read performance by generalizing RocksDB’s Version concept to include the set of live blob files¹.

5. **Compatibility**: The old BlobDB was incompatible with many widely used RocksDB features and was primarily geared towards FIFO/TTL use cases that can tolerate some data loss². The new BlobDB has feature parity with the core of RocksDB¹.

6. **Configuration**: The new BlobDB can be configured on a per-column family basis using the configuration options `enable_blob_files`, `min_blob_size`, `blob_file_size`, `blob_compression_type`, `enable_blob_garbage_collection`, and `blob_garbage_collection_age_cutoff`³.

In summary, the new integrated BlobDB is a significant improvement over the old Stackable BlobDB in terms of API, consistency, performance, compatibility, and configuration¹²³.

Source: Conversation with Bing, 11/15/2023
(1) Integrated BlobDB | RocksDB. https://rocksdb.org/blog/2021/05/26/integrated-blob-db.html.
(2) BlobDB · facebook/rocksdb Wiki · GitHub. https://github.com/facebook/rocksdb/wiki/BlobDB.
(3) Does rocksdb have two blobdb interfaces? - Google Groups. https://groups.google.com/g/rocksdb/c/hrXOP4fCOCs.
(4) BlobDB · facebook/rocksdb Wiki · GitHub. https://github.com/facebook/rocksdb/wiki/BlobDB/f268c1ff9ac2bc0f6157dd897acd3a54390c5131.
```




[Todo] Get total file size from sstfilemanagerimpl instead of getting it from bash command.
Call GetTotalSize()
sst_file_manager is in immutable_db_options_;
[Status: Not started]


[Todo] Find out the root cause for the situation that only one label is generated for all keys.
[Done]

[Todo] Need to check key range id.
Possible root cause. 
1. The model is good, the problem comes from the data we feed into the model in the run time.
    because timestamp has changed from what we feed into the model in the training process. 

2. features? I have not check the features we generated during the run time.

3. key ranges ? I have not check if the key range id return from the function is what 
Need to log key features so that I can check offiline.
we expect.
Need to learn to write a custom reader and writer again.

Need to check if deafult_cf->GetComparator() works expectedly for user key comparison

Experiment results show that all write keys in op trace is classified as 0 
which is not what I expect. So next step is to figure out why this happens 
and fix it in our rocksdb code.
```
key range: 0, count: 2499190
```
Turns out it's not that key range id function return 0 for each key,
it's that I didn't load key ranges data and didn't make key range id 
function work. Start doing this again.
```
key range: 13, count: 25037
key range: 92, count: 24837
key range: 33, count: 25022
key range: 79, count: 25049
key range: 56, count: 25082
key range: 85, count: 25026
key range: 26, count: 24991
key range: 93, count: 25154
key range: 64, count: 25073
key range: 6, count: 24989
key range: 65, count: 25065
key range: 60, count: 25014
key range: 1, count: 25154
key range: 66, count: 24907
key range: 95, count: 24952
key range: 58, count: 25098
key range: 87, count: 24904
key range: 47, count: 24851
key range: 75, count: 25173
key range: 16, count: 24896
key range: 94, count: 25143
key range: 23, count: 24864
key range: 46, count: 25038
key range: 2, count: 25174
key range: 67, count: 25054
key range: 39, count: 25038
key range: 27, count: 25017
key range: 29, count: 24818
key range: 50, count: 24792
key range: 49, count: 25063
key range: 84, count: 24937
key range: 74, count: 25020
key range: 48, count: 24900
key range: 22, count: 25102
key range: 96, count: 25081
key range: 38, count: 24827
key range: 44, count: 24990
key range: 43, count: 24995
key range: 78, count: 25043
key range: 76, count: 24974
key range: 21, count: 24996
key range: 59, count: 25020
```
Get key range results. Actually it works as expected.
Another problem is that I generate key ranges based on
all keys in op instead of each unique user key. -> no this problem
It's strange that each key range has equal amount of keys written.
Difference between accuracy and precision
```
Actually, accuracy and precision are different concepts in the context of machine learning and statistics:

- **Accuracy** is a measure of how many predictions your model got right out of all predictions. It's calculated as `(True Positives + True Negatives) / (True Positives + False Positives + True Negatives + False Negatives)`. In other words, it's the proportion of true results (both true positives and true negatives) among the total number of cases examined.

- **Precision**, also known as the positive predictive value, is the proportion of true positive predictions among all positive predictions. It's calculated as `True Positives / (True Positives + False Positives)`. Precision is about being precise. So, even if you managed to capture only one positive result, and there are no false positives, then you are 100% precise.

In summary, accuracy measures the overall correctness of the classifier, while precision measures the classifier's exactness. Low precision indicates a high number of false positives, low accuracy indicates a high number of false positives and false negatives.
```

Write all prediction scores which are double value to log file and 
found that only first value is non zero. Need to read doc about lightgbm
and then update my logics in current implementation to put the values in
right blob_file_builder
```
0x0000000000441501303030303030303030303030 1697375786227496 90 0 0.814 0.000 0
0x0000000000441501303030303030303030303030 1697375786227496 90 0 0.814 0.000 0
0x0000000000441501303030303030303030303030 1697375786227496 90 0 0.814 0.000 0
0x00000000000A2812303030303030303030303030 1697375786228141 14 0 0.814 0.000 0
0x000000000040BDC1303030303030303030303030 1697375786228366 85 0 0.814 0.000 0
0x000000000040BDC1303030303030303030303030 1697375786228366 85 0 0.814 0.000 0
0x000000000003F69A303030303030303030303030 1697375786228580 6 0 0.812 0.000 0
0x000000000003F69A303030303030303030303030 1697375786228580 6 0 0.812 0.000 0
0x0000000000045C75303030303030303030303030 1697375786228760 6 0 0.812 0.000 0
0x0000000000045C75303030303030303030303030 1697375786228760 6 0 0.812 0.000 0
```


```
 545266 0.005
  51629 0.006
   3730 0.008
   1862 0.012
   2146 0.013
   2326 0.022
    563 0.032
    903 0.034
   1011 0.043
    856 0.046
     29 0.051
    256 0.052
   2570 0.053
    276 0.054
   1026 0.055
     81 0.056
    380 0.057
    116 0.058
    199 0.059
    219 0.060
```
So what does zero mean? Does it mean long or short lifetime?
zero means short lifetime.

There is still only one lifetime class given by model after I fix the
index issue in lifetime_blob_builders selectino.

So now I suspect that it might be because of the timestamp issue.
I only get 0.005 score after adding 1000 sec to original timestamp.
So this proves that timestamp affects the classification results in an
significant degree.

So now I have two choices.
1. discard timestamp features
2. trasnform timestamp to other format so that timestamp can still contribute
to the prediction when timestamp in running is different than that in training set.
```
0x0000000000441501303030303030303030303030 1697375786227496 90 0 0.005 0.000 0
0x0000000000441501303030303030303030303030 1697375786227496 90 0 0.005 0.000 0
0x0000000000441501303030303030303030303030 1697375786227496 90 0 0.005 0.000 0
0x00000000000A2812303030303030303030303030 1697375786228141 14 0 0.005 0.000 0
0x000000000040BDC1303030303030303030303030 1697375786228366 85 0 0.005 0.000 0
0x000000000040BDC1303030303030303030303030 1697375786228366 85 0 0.005 0.000 0
0x000000000003F69A303030303030303030303030 1697375786228580 6 0 0.005 0.000 0
0x000000000003F69A303030303030303030303030 1697375786228580 6 0 0.005 0.000 0
0x0000000000045C75303030303030303030303030 1697375786228760 6 0 0.005 0.000 0
0x0000000000045C75303030303030303030303030 1697375786228760 6 0 0.005 0.000 0
➜  build git:(main) ✗ cut -d ' ' -f 5,5 ./model_output.txt | sort | uniq -c
5000000 0.005
```

Prediction results after adding 10000 micro sec to the timestamp
fed into model. It's quite normal.
```
545392 0.005
  51610 0.006
   3730 0.008
   1862 0.012
   2146 0.013
   2326 0.022
    563 0.032
    903 0.034
   1011 0.043
    856 0.046
     29 0.051
    285 0.052
   2548 0.053
    292 0.054
   1019 0.055
     84 0.056
    380 0.057
    116 0.058
    199 0.059
    219 0.060
   1992 0.061
    981 0.062
   3421 0.063
   1787 0.064
   1800 0.065
   2372 0.066
   2700 0.067
   1931 0.068
   2139 0.069
   5522 0.070
   2935 0.071
   4111 0.072
   1034 0.073
    841 0.074
   3728 0.075
```
[Status: Done]



First test results even without model prediction

with_model
wamp is 2.1
```
0		71MB	2GB	2.2	2.1	5.4	264	264	5	2	418629665.0						419	0.0	0	0.5	0.0	0.4	replay.t1.s1	2023-11-16T12:14:13	8.0.0		5288d1b35a	0.9
```
wamp is 3.1
without gc 
```
11943	(	69MB	2GB	4.2	3.1	10.3	100	100	2	4	83.7	22.2	53	76	144	793581122	419	0.0	0	0.3	0.0	0.3	mixgraph.t1.s1	2023-10-15T21:16:26	8.0.0		5288d1b35a	0.9
```


I think I could just not write key feature tracer but test the model 
prediction performance by just loading the model and then feed in
the data from the original no model trace.

OR I can just write down the features in memory vector and then simply write them out
for each flush job.

[Todo] Load model in cpp to test if python and cpp code work the same way and 
get the same results
It's the same. 
<Status: Done>

I will first test model prediction with cpp code. 
And then I may write keyfeatures logging to see what's wrong with my code
Current op trace header
```
[hex_key, op_tyep, cf_id, value_size, timestamp, sequence, write_rate_mb_per_sec]
```
so now I can get hex_key and write_rate_mb_per_sec from op trace.
And then I can load key ranges from  key_ranges.txt

I get to know what's my favorite coding style.
I tend to first come up with a initial thought about code structure,
and then I will start doing implementation with my initial blur idea.
It's not perfect.
I will first gather essential piece of code and code piece that's not 
so clear to me to build a more concrete code structure .
So now I have more clear view of the final code implementation.
The next step is to do  build  process to correct grammar and compilation
errors.
And then I will continue this loop.

Learn about object oriented programming technique from TraceAnalyzer.
```
record->Accept(Handler, TraceRecordResult*);
```
record is a class that can be subclassed according to trace type such 
as write or read.
Accept() is also a abstrct function. It leaves handler implementation to
users.
WriteQueryTraceRecord inherits record and implments Accept() 
 This Accept() just calls handler->Handle() which is implemented by TraceAnalyzer
TraceAnalyzer also implement Handler interface so that TraceAnalyzer  
`TraceAnalyzer::Handle(WriteQueryTraceRecord& record, TraceRecordResult*) ` calls 
`batch.IterateWithStartSequence(Handler* handler)`
This Handler is another handler which deal with how to write trace info to file.
TraceAnalyzer also implement this Handler interface as well.
`Handler::PutCFWithFeatures()` is another interface and used in 
`IterateWithStartSequence(WriteBatch* wb, WriteBatch::Handler* handler, ...)`
So lots of interface function calling in this process.

[Todo] Need to check out replay code. I suspect that writewithsequence trace
type is not written into rocksdb in replay. 
Does replay accurately read records and put keys into rocksdb?
Because I use different value type .
Checked the replayer code and writewithsequence trace records are 
also written to rocksdb. So no loss of writes.
```
Status TraceExecutionHandler::Handle(
    const WriteQueryTraceRecord& record,
    std::unique_ptr<TraceRecordResult>* result) {
  if (result != nullptr) {
    result->reset(nullptr);
  }
  uint64_t start = clock_->NowMicros();

  WriteBatch batch(record.GetWriteBatchRep().ToString());
  Status s = db_->Write(write_opts_, &batch);

  uint64_t end = clock_->NowMicros();

  if (s.ok() && result != nullptr) {
    result->reset(new StatusOnlyTraceExecutionResult(s, start, end,
                                                     record.GetTraceType()));
  }

  return s;
}
```
Keys are written into rocksdb.
[Done]


confusion matrics in model performance 
```
The matrix that contains True Positives (TP), True Negatives (TN), False Positives (FP), and False Negatives (FN) is known as a **Confusion Matrix**¹². It's a specific table layout that allows visualization of the performance of an algorithm, typically a supervised learning one¹². Each row of the matrix represents the instances in an actual class while each column represents the instances in a predicted class, or vice versa¹². It's a powerful tool for measuring the reliability of your model¹².

Source: Conversation with Bing, 11/21/2023
(1) Confusion matrix - Wikipedia. https://en.wikipedia.org/wiki/Confusion_matrix.
(2) Confusion Matrix in Machine Learning - GeeksforGeeks. https://www.geeksforgeeks.org/confusion-matrix-machine-learning/.
(3) How to find TP,TN, FP and FN values from 8x8 Confusion Matrix. https://stats.stackexchange.com/questions/134486/how-to-find-tp-tn-fp-and-fn-values-from-8x8-confusion-matrix.
(4) Taking the Confusion Out of Confusion Matrices | by Allison Ragan .... https://towardsdatascience.com/taking-the-confusion-out-of-confusion-matrices-c1ce054b3d3e.
```



[Todo] Increase timestamp of records in trace file and do prediction again 
to see how time stamp affect the prediction results.
All keys are classified to zero after increase timestamp 10 million microsecs.
The conclusion is that current timestamp do significantly affect the classification
results.
[Status: Done]



[Todo] Train model without timestamp feature and check the performance.
Get really low recall for short lifetime keys.
Threshold for distinquishing short and long is 50s
Even with one key range feature we still get relatively high precision 
for short and long lifetime keys.
I think all of keys are classified as long. LOL.
```
Starting predicting...
              precision    recall  f1-score   support

           0       0.65      0.05      0.09     53058
           1       0.72      0.99      0.83    130775

    accuracy                           0.72    183833
   macro avg       0.69      0.52      0.46    183833
weighted avg       0.70      0.72      0.62    183833

0.6860720187413802
```

Need to fix bug in which there is segmentation faultwhile 
accessing blob_file_meta.
blob file number is 36 but there is no 36 blob file 
in db dir.
Need to find out why 36 is deleted from VersionStorageInfo
```
gdb) p blob_index.file_number()
$2 = 36
```
Don't know if write_rate_mb_per_sec is the same as that in op trace file.


Creation information for blob file 36
```
2023/11/22-21:52:35.676132 1003597 EVENT_LOG_v1 {"time_micros": 1700661155676123, "cf_name": "default", "job": 8, "event": "blob_file_creation", "file_number": 36, "total_blob_count": 52, "total_blob_bytes": 45008, "file_checksum": "", "file_checksum_func_name": "Unknown", "status": "OK"}
2023/11/22-21:52:35.676148 1003597 [db/blob/blob_file_builder.cc:419] [default] [JOB 8] Generated blob file #36: 52 total blobs, 45008 total bytes, lifetime label: 0
2023/11/22-21:52:35.677490 1003597 EVENT_LOG_v1 {"time_micros": 1700661155677485, "cf_name": "default", "job": 8, "event": "blob_file_creation", "file_number": 35, "total_blob_count": 113585, "total_blob_bytes": 97878460, "file_checksum": "", "file_checksum_func_name": "Unknown", "status": "OK"}
2023/11/22-21:52:35.677503 1003597 [db/blob/blob_file_builder.cc:419] [default] [JOB 8] Generated blob file #35: 113585 total blobs, 97878460 total bytes, lifetime label: 1
2023/11/22-21:52:35.677842 1003597 EVENT_LOG_v1 {"time_micros": 1700661155677819, "cf_name": "default", "job": 8, "event": "table_file_creation", "file_number": 34, "file_size": 3998763, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 465269, "largest_seqno": 581610, "table_properties": {"data_size": 3846499, "index_size": 9111, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 142085, "raw_key_size": 3181836, "raw_average_key_size": 28, "raw_value_size": 1018986, "raw_average_value_size": 8, "num_data_blocks": 472, "num_entries": 113637, "num_filter_entries": 113637, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1700661131, "oldest_key_time": 1700661131, "file_creation_time": 1700661150, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "f60ea76d-6d38-4cee-a9e4-42580208ce7d", "db_session_id": "QM282AVS1T33D7F4LP1T", "orig_file_number": 34, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 35}
2023/11/22-21:52:35.677965 1003597 [db/flush_job.cc:1035] [default] [JOB 8] Flush lasted 5065610 microseconds, and 5061408 cpu microseconds.
2023/11/22-21:52:35.677970 1003597 [db/flush_job.cc:1051] [default] [JOB 8] Flush generated 2blob files  

```

Deletion information for blob file 36
Job started because of ForcedBlobGC
```
2023/11/22-21:52:40.395128 1003596 EVENT_LOG_v1 {"time_micros": 1700661160395120, "job": 14, "event": "compaction_started", "compaction_reason": "ForcedBlobGC", "files_L1": [32], "score": 0.568426, "input_data_size": 3409830, "oldest_snapshot_seqno": -1}

2023/11/22-21:52:43.015951 1003596 [db/compaction/compaction_job.cc:1641] [default] [JOB 14] Generated table #51: 97114 keys, 3409830 bytes, temperature: kUnknown
2023/11/22-21:52:43.016003 1003596 EVENT_LOG_v1 {"time_micros": 1700661163015974, "cf_name": "default", "job": 14, "event": "table_file_creation", "file_number": 51, "file_size": 3409830, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 1, "largest_seqno": 465262, "table_properties": {"data_size": 3279584, "index_size": 7769, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 121413, "raw_key_size": 2719192, "raw_average_key_size": 28, "raw_value_size": 871527, "raw_average_value_size": 8, "num_data_blocks": 402, "num_entries": 97114, "num_filter_entries": 97114, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1700661056, "oldest_key_time": 0, "file_creation_time": 1700661160, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "f60ea76d-6d38-4cee-a9e4-42580208ce7d", "db_session_id": "QM282AVS1T33D7F4LP1T", "orig_file_number": 51, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 50}
2023/11/22-21:52:43.016806 1003596 EVENT_LOG_v1 {"time_micros": 1700661163016801, "cf_name": "default", "job": 14, "event": "blob_file_creation", "file_number": 50, "total_blob_count": 97114, "total_blob_bytes": 81813845, "file_checksum": "", "file_checksum_func_name": "Unknown", "status": "OK"}
2023/11/22-21:52:43.016819 1003596 [db/blob/blob_file_builder.cc:419] [default] [JOB 14] Generated blob file #50: 97114 total blobs, 81813845 total bytes, lifetime label: 0
2023/11/22-21:52:43.017696 1003596 (Original Log Time 2023/11/22-21:52:43.017214) [db/compaction/compaction_job.cc:1714] [default] [JOB 14] Compacted 1@1 files to L1 => 3409830 bytes
2023/11/22-21:52:43.017702 1003596 (Original Log Time 2023/11/22-21:52:43.017571) [db/compaction/compaction_job.cc:867] [default] compacted to: files[1 8 0 0 0 0 0 0] max score 0.57, MB/sec: 32.5 rd, 1.3 wr, level 1, files in(0, 1) out(1 +0 blob) MB in(0.0, 3.3 +78.0 blob) out(3.3 +0.0 blob), read-write-amplify(1.1) write-amplify(0.0) OK, records in: 97114, records dropped: 0 output_compression: NoCompression
2023/11/22-21:52:43.017705 1003596 (Original Log Time 2023/11/22-21:52:43.017577) [db/compaction/compaction_job.cc:892] [default] Blob file summary: head=38, tail=35
2023/11/22-21:52:43.017712 1003596 (Original Log Time 2023/11/22-21:52:43.017601) EVENT_LOG_v1 {"time_micros": 1700661163017582, "job": 14, "event": "compaction_finished", "compaction_time_micros": 2621714, "compaction_time_cpu_micros": 2619979, "output_level": 1, "num_output_files": 1, "total_output_size": 3409830, "num_input_records": 97114, "num_output_records": 97114, "num_subcompactions": 1, "output_compression": "NoCompression", "num_single_delete_mismatches": 0, "num_single_delete_fallthrough": 0, "lsm_state": [1, 8, 0, 0, 0, 0, 0, 0], "blob_file_head": 38, "blob_file_tail": 35}
2023/11/22-21:52:43.017782 1003596 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme0n1/mlsm/test_blob/with_model_gc_1.0_0.8/000036.blob immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2023/11/22-21:52:43.017789 1003596 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 14] Delete /mnt/nvme0n1/mlsm/test_blob/with_model_gc_1.0_0.8/000036.blob type=10 #36 -- OK
2023/11/22-21:52:43.017797 1003596 EVENT_LOG_v1 {"time_micros": 1700661163017794, "job": 14, "event": "blob_file_deletion", "file_number": 36}
```
Try to know which files are being compacted in this gc.
This file has link to blob file 36.

I saw a filemetadata that has oldest blob file number 35.

gdb process was shutdown by my mistake.
Now the missing blob file number is 38.
Need to figure out how blob file is linked and unlinked from sst.
I think oldest blob file number in each sst is updated in ApplyBlobfileDeletion()?
linkedsst in each blob file is updated in ApplyFileAddition()?
So how does this blobfile:38 is deleted?

```
1/23-15:32:42.657540 698368 [db/compaction/compaction_job.cc:1641] [default] [JOB 14] Generated table #52: 97114 keys, 3409848 bytes, temperature: kUnknown
2023/11/23-15:32:42.657591 698368 EVENT_LOG_v1 {"time_micros": 1700724762657563, "cf_name": "default", "job": 14, "event": "table_file_creation", "file_number": 52, "file_size": 3409848, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 1, "largest_seqno": 465262, "table_properties": {"data_size": 3279606, "index_size": 7765, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 121413, "raw_key_size": 2719192, "raw_average_key_size": 28, "raw_value_size": 871527, "raw_average_value_size": 8, "num_data_blocks": 402, "num_entries": 97114, "num_filter_entries": 97114, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1700724655, "oldest_key_time": 0, "file_creation_time": 1700724759, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "80e5b69b-af8d-4869-b8f0-9521a51b3a76", "db_session_id": "HAXRW2BTBV4ZI0TP7O2E", "orig_file_number": 52, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 51}
2023/11/23-15:32:42.658571 698368 EVENT_LOG_v1 {"time_micros": 1700724762658565, "cf_name": "default", "job": 14, "event": "blob_file_creation", "file_number": 51, "total_blob_count": 97114, "total_blob_bytes": 81813853, "file_checksum": "", "file_checksum_func_name": "Unknown", "status": "OK"}
2023/11/23-15:32:42.658582 698368 [db/blob/blob_file_builder.cc:419] [default] [JOB 14] Generated blob file #51: 97114 total blobs, 81813853 total bytes, lifetime label: 0
2023/11/23-15:32:42.659481 698368 (Original Log Time 2023/11/23-15:32:42.658933) [db/compaction/compaction_job.cc:1714] [default] [JOB 14] Compacted 1@1 files to L1 => 3409848 bytes
2023/11/23-15:32:42.659488 698368 (Original Log Time 2023/11/23-15:32:42.659341) [db/compaction/compaction_job.cc:867] [default] compacted to: files[1 7 0 0 0 0 0 0] max score 0.57, MB/sec: 31.4 rd, 1.3 wr, level 1, files in(0, 1) out(1 +0 blob) MB in(0.0, 3.3 +78.0 blob) out(3.3 +0.0 blob), read-write-amplify(1.1) write-amplify(0.0) OK, records in: 97114, records dropped: 0 output_compression: NoCompression
2023/11/23-15:32:42.659491 698368 (Original Log Time 2023/11/23-15:32:42.659347) [db/compaction/compaction_job.cc:892] [default] Blob file summary: head=40, tail=37
2023/11/23-15:32:42.659499 698368 (Original Log Time 2023/11/23-15:32:42.659373) EVENT_LOG_v1 {"time_micros": 1700724762659352, "job": 14, "event": "compaction_finished", "compaction_time_micros": 2716971, "compaction_time_cpu_micros": 2714167, "output_level": 1, "num_output_files": 1, "total_output_size": 3409848, "num_input_records": 97114, "num_output_records": 97114, "num_subcompactions": 1, "output_compression": "NoCompression", "num_single_delete_mismatches": 0, "num_single_delete_fallthrough": 0, "lsm_state": [1, 7, 0, 0, 0, 0, 0, 0], "blob_file_head": 40, "blob_file_tail": 37}
2023/11/23-15:32:42.660299 698368 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme0n1/mlsm/test_blob/with_model_gc_1.0_0.8/000038.blob immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2023/11/23-15:32:42.660319 698368 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 14] Delete /mnt/nvme0n1/mlsm/test_blob/with_model_gc_1.0_0.8/000038.blob type=10 #38 -- OK
2023/11/23-15:32:42.660330 698368 EVENT_LOG_v1 {"time_micros": 1700724762660327, "job": 14, "event": "blob_file_deletion", "file_number": 38}
2023/11/23-15:32:42.661042 698368 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme0n1/mlsm/test_blob/with_model_gc_1.0_0.8/000034.sst immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2023/11/23-15:32:42.661050 698368 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 14] Delete /mnt/nvme0n1/mlsm/test_blob/with_model_gc_1.0_0.8/000034.sst type=2 #34 -- OK
2023/11/23-15:32:42.661057 698368 EVENT_LOG_v1 {"time_micros": 1700724762661055, "job": 14, "event": "table_file_deletion", "file_number": 34}
```
delete sst:34 and blob:38 


job:7 generate blob:33 and sst:34 and sst:39

job:8
blob:38 is generated with sst:36. blob:37 is generated as well.
blob:38  lifetime:0
blob:37 lifetime:1

job:14 deletes blob:38 and blob:33 and sst:34
Why blob:38 is deleted?
blob_file_head is 40 and blob_file_tail is 37 but blob_file:51 is created in this job
```
2023/11/23-15:32:42.659499 698368 (Original Log Time 2023/11/23-15:32:42.659373) EVENT_LOG_v1 {"time_micros": 1700724762659352, "job": 14, "event": "compaction_finished", "compaction_time_micros": 2716971, "compaction_time_cpu_micros": 2714167, "output_level": 1, "num_output_files": 1, "total_output_size": 3409848, "num_input_records": 97114, "num_output_records": 97114, "num_subcompactions": 1, "output_compression": "NoCompression", "num_single_delete_mismatches": 0, "num_single_delete_fallthrough": 0, "lsm_state": [1, 7, 0, 0, 0, 0, 0, 0], "blob_file_head": 40, "blob_file_tail": 37}
```

suspect that some blobs are frocebly deleted at the end of the job.
This doesn't make sense. blob:37 still exists.

PurgeObsoleteFiles(job_context) is called in BackgroundCompaction()
Try to find out how blob:38 is pushed into deleted_files
```
  versions_->GetObsoleteFiles(
      &job_context->sst_delete_files, &job_context->blob_delete_files,
      &job_context->manifest_delete_files, job_context->min_pending_output);


```

```
void VersionSet::GetObsoleteFiles(std::vector<ObsoleteFileInfo>* files,
                                  std::vector<ObsoleteBlobFileInfo>* blob_files,
                                  std::vector<std::string>* manifest_filenames,
                                  uint64_t min_pending_output) {
  assert(files);
  assert(blob_files);
  assert(manifest_filenames);
  assert(files->empty());
  assert(blob_files->empty());
  assert(manifest_filenames->empty());

  std::vector<ObsoleteFileInfo> pending_files;
  for (auto& f : obsolete_files_) {
    if (f.metadata->fd.GetNumber() < min_pending_output) {
      files->emplace_back(std::move(f));
    } else {
      pending_files.emplace_back(std::move(f));
    }
  }
  obsolete_files_.swap(pending_files);

  std::vector<ObsoleteBlobFileInfo> pending_blob_files;
  for (auto& blob_file : obsolete_blob_files_) {
    if (blob_file.GetBlobFileNumber() < min_pending_output) {
      blob_files->emplace_back(std::move(blob_file));
    } else {
      pending_blob_files.emplace_back(std::move(blob_file));
    }
  }
  obsolete_blob_files_.swap(pending_blob_files);

  obsolete_manifests_.swap(*manifest_filenames);
}
```
When is blob:38 pushed into obsolete_blob_files_?


obsolete_blob_files_ is updated in AddObsolteBlobFile() and AddObsolteBlobFileWithLifetime()
But I don't think AddObsolteBlobFile() is called.
AddObsolteBlobFileWithLifetime() is called when SharedBlobFileMetaData is deleted.
SharedBlobFileMetaData is created in ApplyBlobFileAddition() function()

contents of mutable_blob_file_metas_ is accessed in MergeBlobFileMetas()
So where is MergeBlobFileMetas() is called?

```

void VersionBuilder::Rep::SaveBlobFilesTo(VersionStorageInfo* vstorage) const {
    GetMinOldestBlobFileNumber()
        MergeBlobFileMetas(, process_base, process_mutable, process_both)
            auto base_it = base_vstorage_->GetBlobFileMetaDataLB()
            auto mutable_it = mutable_blob_file_metas_.lower_bound(first_blob_file)
```
This is problematic.
This returned min blob file number in all blob_files.
Why this GetMinOldestBlobFileNumber() is called?
I think a blob file will be pushed to obsolete_blob_files_ once there is
no linked_sst in it.
Difference between BlobFileMetaData and MutableBlobFileMetaData
BlobFileMetaData can not be updated.
MutableBlobFileMetaData has interface that can link sst and unlink sst.
It can also add garbage

VersionBuilder  has base_vstorage_ and then it saves all blob files to

I think LinkSst() is called in ApplyFileAddition in rocksdb
Where is UnlinkSst() is called?
UnlinkSst() is called in ApplyFileDeletion()

Print all blob file number for each job. and blob file numbers for each lifetime label.

Thinking about how oldest_blob_file_number is updated for each FileMetaData

Maybe it's that a FileMetaData doesn't know which blob file is the oldest to it
because it could be that there is two blob file opening at the same time while 
compaction iterator is traversing. 

oldest_blob_file_number of FileMetaData is updated at FileMetaData::UpdateBoundaries()

```
Status FileMetaData::UpdateBoundaries(const Slice& key, const Slice& value,
                                      SequenceNumber seqno,
                                      ValueType value_type) {
  if (value_type == kTypeBlobIndex) {
    BlobIndex blob_index;
    const Status s = blob_index.DecodeFrom(value);
    if (!s.ok()) {
      return s;
    }

    if (!blob_index.IsInlined() && !blob_index.HasTTL()) {
      if (blob_index.file_number() == kInvalidBlobFileNumber) {
        return Status::Corruption("Invalid blob file number");
      }

      if (oldest_blob_file_number == kInvalidBlobFileNumber ||
          oldest_blob_file_number > blob_index.file_number()) {
        oldest_blob_file_number = blob_index.file_number();
      }
    }
  }

  if (smallest.size() == 0) {
    smallest.DecodeFrom(key);
  }
  largest.DecodeFrom(key);
  fd.smallest_seqno = std::min(fd.smallest_seqno, seqno);
  fd.largest_seqno = std::max(fd.largest_seqno, seqno);

  return Status::OK();
}
```
So we can make sure that FileMetaData has smallest blob file number. 
But FileMetaData only stores oldest_blob_file_number of all the blob files
it refers to.

Another experiment after change some code.
job 99
sst:263, blob:264 with lifetime:1, blob:265 with lifetime:0.

job 111
delete sst:261 , blob:260, blob:265
looks like blob:265 is not added to the latest version storage because it has no
linksst to 263. It is the oldest blob file with no linkedsst.
I think I find the root cause why this bug happens

Start another experiment and the bug still shows up. 
Analyzed tht log and find out that it's because I don't call 
GetMinOldestBlobFileNumber() for all blob files. So blob files are  
deleted with no linkesst even though there are blob files whose blob file
number are smaller than that with linkedsst.
Solution: call GetMinOldestBlobFileNumber() instead of GetMinOldestBlobFileNumber(lifetime)
There is no segmentation fault after I call GetMinOldestBlobFileNumber() to get 
minimum oldest blob file number of blob file that has link to sst.
Great;
[Status: Done]


[Todo] Need to log garbage count for each blob file in current garbage collection process
[Status: Not started]

[Todo] Try to run experiments to get comparison between our method and baseline method interm of 
write amplification and read write rate.
[Status: Not started]




[Todo] log blob files and sst files for each garbage collection.
May need to change gc implementation as well. Current gc implementation 
is not good for our idea.

We can get garbage collected blob files from LOG. And I added blob files
for each lifetime classification as well. 
So next step is to change gc implementation. This might significant impact 
the read and write rate of rocksdb. 
It would be better if we can show that for these two implementation we all achieve
better performance than baseline method then we are good.

[Status: Done ]


[Todo] Get relationship between key range and their key lifetime

Get valid duration distribution all write keys.
And find that 50% of keys have lifetime shorter than 100 secs.
80% of keys have lifetime shorter than 200 secs.
This is very different from what we get from compaction trace.
Need to retrain the model to do evaluation..

count of write op with overwrite.
```
919164 op_valid_duration.txt
```

count of all write ops
So more than 50% of keys don't have a overwrite in our trace 
which means more than 50% of keys are valid of all write.
```
 awk '$2 == 1 {count++} END {print count}' ./trace-human_readable_trace.txt
2499190
```

No significant difference for lifetime distribution across different key ranges 
for this workload.
Let's try some other workload.

Didn't write op_valid_duration.txt to test_blob_no_model folder. Do it again.
I did a sanity check for the output op_valid_duration.txt file and found that 
the distribution of lifetime is not updated.

lifetime distribution for keys after adjusting the lifetime threshold from 0.5*1e8 to 2.0*1e8
```
  with_gc_1.0_0.8 awk 'BEGIN {short_count=0; long_count=0;} {if ($8 == 0) {short_count++} else {long_count++} } END {print "shot_count:" short_count " long_count:" long_count}' ./op_valid_duration.txt
shot_count:738175 long_count:180989
```
Let's train the model and then test the performance again.
no keys are classified as long with new dataset which is very bad.
```
[LightGBM] [Info] Number of positive: 144514, number of negative: 590816
[LightGBM] [Debug] Dataset::GetMultiBinFromAllFeatures: sparse rate 0.002247
[LightGBM] [Debug] init for col-wise cost 0.005916 seconds, init for row-wise cost 0.017670 seconds
[LightGBM] [Warning] Auto-choosing row-wise multi-threading, the overhead of testing was 0.010846 seconds.
You can set `force_row_wise=true` to remove the overhead.
And if memory is not enough, you can set `force_col_wise=true`.
[LightGBM] [Debug] Using Dense Multi-Val Bin
[LightGBM] [Info] Total Bins 335
[LightGBM] [Info] Number of data points in the train set: 735330, number of used features: 2
[LightGBM] [Info] [binary:BoostFromScore]: pavg=0.196529 -> initscore=-1.408128
[LightGBM] [Info] Start training from score -1.408128

/home/zt/.local/lib/python3.10/site-packages/sklearn/metrics/_classification.py:1469: UndefinedMetricWarning:Precision and F-score are ill-defined and being set to 0.0 in labels with no predicted samples. Use `zero_division` parameter to control this behavior.
  _warn_prf(average, modifier, msg_start, len(result))
/home/zt/.local/lib/python3.10/site-packages/sklearn/metrics/_classification.py:1469: UndefinedMetricWarning: Precision and F-score are ill-defined and being set to 0.0 in labels with no predicted samples. Use `zero_division` parameter to control this behavior.
  _warn_prf(average, modifier, msg_start, len(result))
/home/zt/.local/lib/python3.10/site-packages/sklearn/metrics/_classification.py:1469: UndefinedMetricWarning: Precision and F-score are ill-defined and being set to 0.0 in labels with no predicted samples. Use `zero_division` parameter to control this behavior.
  _warn_prf(average, modifier, msg_start, len(result))
              precision    recall  f1-score   support

           0       0.80      1.00      0.89    147359
           1       0.00      0.00      0.00     36474

    accuracy                           0.80    183833
   macro avg       0.40      0.50      0.44    183833
weighted avg       0.64      0.80      0.71    183833

/home/zt/.local/lib/python3.10/site-packages/sklearn/metrics/_classification.py:1469: UndefinedMetricWarning: Precision is ill-defined and being set to 0.0 in labels with no predicted samples. Use `zero_division` parameter to control this behavior.
  _warn_prf(average, modifier, msg_start, len(result))
0.40079583099878696
```
[Status: Done ]



[Todo] Implement key range seletion algorithim in Leaper
[Status: Not started]


[Todo] Read machine learning to predict latency for io request in ssd paper 
to learn how to get reflection point.
This can help us to get reflection point for lifetime distribution as well.
[Status: Not started]

[Todo]
Try adding more features to improve classification accuracy and recall rate.
- time elapse for cur write interval and previous write 
- read/write count/rate of the previous interval .
add ../mlsm_scripts/op_valid_duraion.py script to plot figure 
for lifetime for each key range.
add ../mlsm_scripts/op_key_range_read_write_rate.py to get 
read/write rate for different key range.

- time elapse from the start of the database
Added time elapse feature data to train model. Not working at all.
[Status: Not started]

[Todo]
Garbage definition in writing paper.
[Status: Not started]




[Todo] Download twitter memcached trace 
https://github.com/twitter/cache-trace
https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/open_source/
[Status: Done]

[Todo] Analyze twitter memcached trace .
trace link:  https://github.com/twitter/cache-trace
Get key range information.
There is namespace for each key. Maybe we can use this as feture.
Get lifetime for each key range.
Get read/write rate for each key range.

trace format :
timestamp: the time when the cache receives the request, in sec
anonymized key: the original key with anonymization
key size: the size of key in bytes
value size: the size of value in bytes
client id: the anonymized clients (frontend service) who sends the request
operation: one of get/gets/set/add/replace/cas/append/prepend/delete/incr/decr
TTL: the time-to-live (TTL) of the object set by the client, it is 0 when the request is not a write request.
Note that during key anonymization, we preserve the namespaces, for example, if the anonymized key is nz:u:eeW511W3dcH3de3d15ec, the first two fields nz and u are namespaces, note that the namespaces are not necessarily delimited by :, different workloads use different delimiters with different number of namespaces.

A sample of the traces are attached under samples.

Read paper A large scale analysis of hundreds of in-memory cache clusters at Twitter
to know more about write heavy traces in this dataset.

read/write ratio in cluster12.sort trace dataset.
write:read = 4:1  which is really write heavy.
```
➜  cluster head -n1000000 ./cluster12.sort |  cut -d ',' -f 6,6  | sort | uniq -c
 198797 get
 801203 set
➜  cluster head -n5000000 ./cluster12.sort |  cut -d ',' -f 6,6  | sort | uniq -c
1006713 get
3993287 set
➜  cluster head -n10000000 ./cluster12.sort |  cut -d ',' -f 6,6  | sort | uniq -c
2032644 get
7967356 set
```

There is no many writes for each namespace in twitter trace
```
first_name_space count:  1581831
set group count:  1679043


```

Write trace is not skewed enough
```
➜  twitter git:(main) ✗ python3 op_key_valid_duration.py
set_group_sort count:  79607
group by key_id done
key_id_group count:  78721
➜  twitter git:(main) ✗
➜  twitter git:(main) ✗
➜  twitter git:(main) ✗
➜  twitter git:(main) ✗ python3 op_key_valid_duration.py
set_group_sort count:  801203
group by key_id done
key_id_group count:  790048
```
overwrite ratio is only 1.2%

Need to check other write heavy workload.
[Satus: Done]


[Todo] Get overwrite ratio for wiki dataset.
wiki trace link
https://github.com/sunnyszy/lrb
```
 head -n10000000 ./wiki2019_remapped.tr |  cut -d ' ' -f 4,4  | sort | uniq -c
4040075 0
5678747 1
 131013 2
  18926 3
 131203 4
     20 5
      4 6
     12 7
```
trace format 
```
Request traces are expected to be in a space-separated format with 3 columns and additionally columns for extra features.

time should be a long long int, but can be arbitrary (for future TTL feature, not currently in use)
id should be a long long int, used to uniquely identify objects
size should be uint32, this is object's size in bytes
extra features are optional uint16 features. LRB currently interprets them as categorical features (e.g., object type).
```

```
time	id	size	[extra_feature(s)]
1	1	120	
2	2	64	
3	1	120	
4	3	14	
4	1	120
```
too little information.

This wiki request trace is not good for our task.
Because nearly 50% of request are labelled as 1
and 28% of keys are accessed more than once. 
This is not true from what I get from twitter cache 
access trace.
```
tail_100 :          time  key_id  value_size  op_type  key_range_idx
965270   555      68       15328        1              0
68         0      69       15446        1              0
70         0      71       10594        1              0
71         0      72     2093687        1              0
72         0      73      330152        1              0
...      ...     ...         ...      ...            ...
339556   194     251        9785        1              0
340139   195     251        9785        1              0
340507   195     251        9785        1              0
342459   196     251        9785        1              0
343010   196     251        9785        1              0

[900 rows x 5 columns]
key_149           time  key_id  value_size  op_type  key_range_idx
148        0     149        4978        1              0
7050       4     149        4978        1              0
50124     28     149        4978        1              0
51077     29     149        4978        1              0
115718    65     149        4978        1              0
164790    94     149        4978        1              0
234043   133     149        4978        1              0
282017   161     149        4978        1              0

group by key_id done
set_group_sort count:
 570895
key_id_group count:  411459
```
28% overwrite

Need to see if there is correlation of lifetime in each key range.
[Status: Paused ]



[Todo]
Analyze ibm cos trace dataset.
Get overwrite ratio for keys
Need to write shell script or python script
to get overwrite rate for multiple trace data files.
```
Processing IBMObjectStoreTrace000Part0 file...                                                                                                                                           [31/1815]  10800 REST.COPY.OBJECT
  23566 REST.DELETE.OBJECT
 962187 REST.GET.OBJECT
44760170 REST.HEAD.OBJECT
  66010 REST.PUT.OBJECT
Processing IBMObjectStoreTrace001Part0 file...
 468959 REST.GET.OBJECT
 479438 REST.HEAD.OBJECT
 629333 REST.PUT.OBJECT
Processing IBMObjectStoreTrace002Part0 file...
    119 REST.DELETE.OBJECT
 376250 REST.GET.OBJECT
    126 REST.PUT.OBJECT
Processing IBMObjectStoreTrace003Part0 file...
   1325 REST.COPY.OBJECT
1487695 REST.GET.OBJECT
  12539 REST.HEAD.OBJECT
  12244 REST.PUT.OBJECT
Processing IBMObjectStoreTrace004Part0 file...
9900394 REST.GET.OBJECT
  24313 REST.PUT.OBJECT
Processing IBMObjectStoreTrace005Part0 file...
      6 REST.COPY.OBJECT
 223338 REST.GET.OBJECT
52776655 REST.HEAD.OBJECT
      1 REST.PUT.OBJECT
Processing IBMObjectStoreTrace005Part1 file...
   9806 REST.DELETE.OBJECT
 280718 REST.GET.OBJECT
52699190 REST.HEAD.OBJECT
  10286 REST.PUT.OBJECT
Processing IBMObjectStoreTrace005Part2 file...
     12 REST.COPY.OBJECT
     44 REST.DELETE.OBJECT
 218976 REST.GET.OBJECT
52780868 REST.HEAD.OBJECT
    100 REST.PUT.OBJECT
Processing IBMObjectStoreTrace005Part3 file...
 209348 REST.GET.OBJECT
52790652 REST.HEAD.OBJECT
Processing IBMObjectStoreTrace005Part4 file...
 207990 REST.GET.OBJECT
```

000 trace:
```
Processing IBMObjectStoreTrace000Part0 file...                                                                                            [31/1815]  10800 REST.COPY.OBJECT
  23566 REST.DELETE.OBJECT
 962187 REST.GET.OBJECT
44760170 REST.HEAD.OBJECT
  66010 REST.PUT.OBJECT
➜  ibm_cos_trace awk '{if ($2 == "REST.PUT.OBJECT") {print  }}' ./IBMObjectStoreTrace000Part0 |   cut -d ' ' -f 3,3   | sort | uniq -c | wc -l
64703
```
overwrite rate:2%

```
IBMObjectStoreTrace001Part0 has most writes compared to other trace files.
all put count:
```

```

Processing IBMObjectStoreTrace001Part0 file...
 468959 REST.GET.OBJECT
 479438 REST.HEAD.OBJECT
 629333 REST.PUT.OBJECT

uniq write trace count:
➜  ibm_cos_trace wc -l ibm_trace_001_put_key_count.log
629328 ibm_trace_001_put_key_count.log
```
0 overwrite rate.

Need to check  readme as well.There are multiple files for this ibm trace.
I want to combine all trace files together to see if overwrite rate can 
be higher than 10% in total.

Each trace is from each unique bucket.
So we cannot combine all traces together to analyze the overwrite rate.
Two key ids share same id don't mean they are the same object because they 
are in different buckets.
[Status: Done]




[Todo]
Analyze MSR IO dataset
http://iotta.snia.org/traces/block-io/388
```
3. I/O trace file format

The files are gzipped csv (comma-separated text) files. The fields in
the csv are:

Timestacmp,Hostname,DiskNumber,Type,Offset,Size,ResponseTime

Timestamp is the time the I/O was issued in "Windows filetime"
Hostname is the hostname (should be the same as that in the trace file name)
DiskNumber is the disknumber (should be the same as in the trace file name)
Type is "Read" or "Write"
Offset is the starting offset of the I/O in bytes from the start of the logical
disk.
Size is the transfer size of the I/O request in bytes.
ResponseTime is the time taken by the I/O to complete, in Windows filetime
units.

```
This data format is not very good for rocksdb 
because it doesn't  have object id for each write.
We can create object by composing disk number and offset but it's 
not so real
```
➜  MSR-Cambridge  cut -d ',' -f 4,4  ./hm_0.csv  | sort | uniq -c
1417748 Read
2575568 Write

➜  MSR-Cambridge awk -F ',' '{if ($4 == "Write") {print  }}' ./hm_0.csv  |   cut -d ',' -f 5,5   | sort  | uniq -c  | wc -l
178395
```
93% overwrite ratio for each uniq start offset in ssd.

```
 MSR-Cambridge awk -F ',' '{if ($4 == "Write") {print  }}' ./hm_0.csv  |   cut -d ',' -f 5,5  -r--r--r-- 1 zt zt 139M Oct 30  2008 prxy_0.csv.gz                                               │ | sort  | uniq -c  | wc -l
-r--r--r-- 1 zt zt 2.0G Oct 30  2008 prxy_1.csv.gz                                               │178395
```

```
awk -F ',' '{if ($4 == "Write") {print  }}' ./hm_0.csv  |   cut -d ',' -f 5,5   | sort  | uniq -c  | sort -n -k1,1 |  tee ./hm_0_overwrite_cout.log
  10095 210767872
  10262 211292160
  10263 200953856
  10308 1068385280
  10361 12024070144
  10399 1064436736
  10545 1076269056
  10571 1068384256
  10873 13574994432
  11170 3221770240
  11400 13570785280
  11457 3233579008
  11498 3224940544
  13503 3282030592
  14923 13574999040
  17466 13574987776
  17496 1023483904
  18839 1064435712
  18897 1023476224
  18897 1024086016
  18897 1024087040
  18897 1024091136
  18897 1042001920
  19172 11812225024
  19281 6334689280
  19324 3227922432
  19801 3224608768
  20482 3229863936
  20485 3227648000
  21783 571084800
  21784 1417940992
  21784 432615424
  21784 663928832
  21784 702128128
  21785 1316483072
  37794 1023475712
  42248 3154124800
  42249 3154128896
  56691 200626176
 152769 3154137088
 152770 3154132992
```


![image](https://github.com/BilyZ98/rocksdb_kv_sep/assets/26542149/e5210d3e-85d0-4f2b-92b4-3a8d0412355e)
MSR has multple traces and each trace has different read/write ratio.
![image](https://github.com/BilyZ98/rocksdb_kv_sep/assets/26542149/4ff19299-5b83-459b-b92d-6918508d0ea8)
These traces show write intensive characteristics.
1. prn -> print server
2. hm -> hardware monitoring
3. prxy -> firewall / web proxy 
4. src1 -> source control
5. rsrch -> research proejcts

Need to write python script to build unique key for each write request to offset in each disk.


prxy_0 data set
```
➜  MSR-Cambridge cut -d ',' -f 4,4  ./prxy_0.csv | sort | uniq -c
 383524 Read
12135444 Write
```
number of uniqe offset in prxy_0 dataset
```
➜  msr git:(main) ✗ python3 write_analysis.py
count of write:  12135444
count of read:  383524
count of disknumber_offset:  137388
```
overwrite rate: (12135444 - 137388) / 12135444 = 98.8%
avg of write size:  avg of size:  4759.97633378721


prxy_1 data set read write count:
```
➜  MSR-Cambridge cut -d ',' -f 4,4  ./prxy_1.csv | sort | uniq -c
110414460 Read
58224504 Write
```
overwrite rate: (58224504 -    297490) / 58224504 = 99%
avg of size: 13366.629207798833



prn_0
```
count of write:  4983406
count of read:  602480
avg of size:  9903.85422981792
count of disknumber_offset:  614901

count of write:  4983406
count of read:  602480
count of disknumber_offset:  614901
```
overwrite ratio: 87%


[sub todo] plot Write size cdf
[status: partially done]

[sub todo] relationship between write offset and write size  .
[status: partially done]

[sub todo] relationship between key range and lifetime

[sub todo]  check trace replay code to see if timestamp can be replayed 
timestamp can be replayed
[sub status: Done]

exactly

read write relationship for each offset. cdf? 

[Status: Abandoned]


[Todo]
https://github.com/brianfrankcooper/YCSB/wiki/Running-a-Workload
paper title:SSD-Based Workload Characteristics and Their Performance Implications
What's the discovery about write correlation?
Try YCSB-A which has read/write ratio 50:50
1 billion ops. 50 million writes which is similar to that in prxy_1 in msr dataset.

Total update count:
```
4999945 READ
5000055 UPDATE
```

uniq key count:
```
✗ awk '{if ($1 == "UPDATE") {print }}' ./workloada-run-10000000-10000000.log.formated|   cut -d ' ' -f 2,2   | sort | uniq -c | wc -l
2214153
```
28/50 = 56%  overwrite rate
What's the overwrite count and std of overwrite date?

```
>>> df_count_sort.shape
(3758712, 2)
```
This is different from what I get from command line
Fix the bug and filter write data before count number of write
for each key.
```
➜  ycsb_a git:(main) ✗ python3 -i write_access_count.py
>>> df_count_sort.shape
(2214153, 2)
```

number of keys whose write count is greater than 2
```
>>> selected_rows.shape
(428537, 2)
```
ratio: 19%
[Status: Done]

[Todo] Analyze ssd trace from ycsb_a workload running on rocksdb 

```
 tr -s ' '  < ssdtrace-{00,01}  |  cut -d ' ' -f 7,7  | sort | uniq -c | tee  read_write_count.log
  12798 FWFS
     32 FWS
23190515 N
51202140 R
     58 RA
   2324 RM
76803261 RS
   2411 RSM
   6410 W
  25691 WFS
 150715 WM
3406151 WS
```
Need to process large amount of data
[Status: Done]
read write relationship . how many read for a key before it is rewritten?


1. wrong assumption
2. wrong definition of garbage

I need to find a write heavy workload.

Thinkging about how to get extra storage space to store dataset..

Will be nice if we can have a cloud storage to use or 
dfs to use.
What's the current status of cloud storage.
blob object storage.
cloud database.

[Todo] overwrite ratio in mixed graph trace
Use trace information from mixgraph workload and get that 50% of
the keys are conly accessed once .
I will adjust the distribution parameters to get more keys 
accessed at least twice .

```
➜  with_gc_1.0_0.8 cut -d ' ' -f 2,2  ./trace-human_readable_trace.txt | sort | uniq -c
2500810 0
2499190 1
➜  with_gc_1.0_0.8 awk -F ',''{if ($4 == "Write") {print  }}' ./hm_0.csv  |   cut -d ',' -f 5,5   | sort  | uniq -c  | wc -l
➜  with_gc_1.0_0.8 awk '{if ($2 == "1") {print  }}' ./trace-human_readable_trace.txt |   cut -d ' ' -f 1,1   | sort  | uniq -c  | wc -l
1580027
```
overwrite ratio:
41% overwrite rate


[Status: Done]

[Todo] Analyze UMass trace
https://traces.cs.umass.edu/index.php/Storage/Storage
trace format
```
➜  umass_finalcial head -n10 Financial1.spc
0,303567,3584,w,0.000000
1,55590,3072,w,0.000000
0,303574,3584,w,0.026214
1,240840,3072,w,0.026214
1,55596,3072,r,0.078643
0,303581,3584,w,0.117964
1,55596,3072,w,0.117964
0,303588,3584,w,0.530841
1,55596,3072,w,0.530841
0,303595,3584,w,0.550502
```

```
➜  umass_finalcial cut -d ',' -f 4,4  ./Financial1.spc | sort | uniq -c
1235633 r
4099354 w
```

```
➜  umass_finalcial awk -F ',' '{if ($4 == "w") {print  }}' ./Financial1.spc |   cut -d ',' -f 1,1   | sort  | uniq -c  | wc -l
24
```
Too little data. Abandon this one
[Status: Done]

Write a paper about write statistics in workload ?

What kind of datasets are used in other lsm-tree research paper?


```
https://datasetsearch.research.google.com/search?src=0&query=object%20storage&docid=L2cvMTFrc2tzNmowXw%3D%3D
```


https://www.usenix.org/system/files/hotstorage20_paper_eytan.pdf
This FIFO vs. LRU paper uses MSR trace to evaluate cache perfromance.
So I think we could do that for our rocksdb sotrage enginer as well.
```

Group Name  Traces #    Accesses Millions   Objects Millions    Objects Size Gigabytes
MSR   3   68   24    905
SYSTOR  3   235 154 4538
TPCC    8   94  76  636
IBM COS 99  858 149 161,869
```

*The object sizes also show great variance. The overall
 distribution of object sizes in shown in Figure 3. We see that
 a vast majority of the objects (85%) in the traces are smaller
 than a megabyte, Yet these objects only account for 3% of the
 of the stored capacity. Note that since object storage traces
 reference objects of variable size one should adopt a strategy
 for handling such variable sized data within a cache. In our
 simulation we break large objects into fixed size 4MB blocks
 and treat each one separately at the cache layer. Requests
 smaller than 4MB take up their actual length in the cache.
 This is aligned with the fact that GET requests to the IBM
 COS often include range reads in which only a part of an
 object is read*

How does leaper generate key ranges?
Leaper initializes key ranges size as 10 initially. 
For each key range one bit is used to labelle whether it's accessed 
in current time interval. It then double the size of the key range 
and recomputes the access bit for each new key range.
The key range size exponentially increases, the matrics size decrease,
so the access info decreases. It stops doubling key range size 
once zeros in 2*A / zeros in A < alpha(0.6)


Read Learning relaxed Belady for CDN caching paper to learn how to prepare 
data for model training.
One thing that is interesting is that it builds a learned cache system 
that can continuosly train from new data and upadate online model.  
The paper says that distribution can change over time.

[Todo]
Download lrb code and then check how edc feature data is calculated.

```
max_hash_edc_idx = 65535
uint8_t base_edc_window = 10;
 uint32_t memory_window = 67108864;


    static void set_hash_edc() {
        max_hash_edc_idx = (uint64_t) (memory_window / pow(2, base_edc_window)) - 1;
        hash_edc = vector<double>(max_hash_edc_idx + 1);
        for (int i = 0; i < hash_edc.size(); ++i)
            hash_edc[i] = pow(0.5, i);
    }


       MetaExtra(const uint32_t &distance) {
        _past_distances = vector<uint32_t>(1, distance);
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
            _edc[i] = hash_edc[_distance_idx] + 1;
        }
    }


     edc_windows = vector<uint32_t>(n_edc_feature);
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            edc_windows[i] = pow(2, base_edc_window + i);
        }


      void update(const uint32_t &distance) {
        uint8_t distance_idx = _past_distance_idx % max_n_past_distances;
        if (_past_distances.size() < max_n_past_distances)
            _past_distances.emplace_back(distance);
        else
            _past_distances[distance_idx] = distance;
        assert(_past_distances.size() <= max_n_past_distances);
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= max_n_past_distances * 2)
            _past_distance_idx -= max_n_past_distances;
        for (uint8_t i = 0; i < n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
            _edc[i] = _edc[i] * hash_edc[_distance_idx] + 1;
        }
    }
```
size of hash_edc is 65536
hash_edc = [1, 0.5, 1/4, 1/8, ...]
distance is timestamp diff
size of edc_windows is 10 . I think the meaning of edc_windows is that how long each 
edc_windows cover in terms of time
code understood.
Will reimplement this code in python tomorrow.
Need to check what ？
Need to check what's the unit for the timestamp. millisecond?? s ?  microsecond? 
I will add edc count features to dataset.
The most important thing right now is to run model.
Prediction targeet
1. time 
2. log(time to next request)
3. short and long ( binary or exponentially increase duration )


Need to add 10 more delta columns to features. add 10 more edc columns to features.
Get overflow error. Need to fix this. Fixed. Turns out it's because I write it 
like this `edc += edc * edc_windos + 1`
the correct calculation is `edc = edc * edc_windows + 1`

get edc for read and write as features.

Fixed another bug which causes number of rows in dataframe double because 
of misunderstanding of loc and iloc method.

[Status: Done]


Train model with msr prn_0 data.
binary classification model.
Get a relatively good prediction results with 20 feature columns
Another problem is that how to determine the short and long 
if we don't know the threshold unless we get the data. 
We could gather quite amount of data and make the assumption that 
the lifete data distribution is long tail and skewed.
```
              precision    recall  f1-score   support

       False       0.90      0.89      0.90    787778
        True       0.61      0.62      0.62    208904

    accuracy                           0.84    996682
   macro avg       0.75      0.76      0.76    996682
weighted avg       0.84      0.84      0.84    996682

0.754550580551913
```
This is not right. I didn't provide right lifetime because I write dataframe
to csv file concurrently.

Need to get write amplification results with different model.
If we have more than 2 lifetime categories, we need to make sure 
each category blob files are GCed when they are expired.

Generated data file with keys write count is bigger than 1 this is biased.
Need to gnerate another file with all write accesses. I will make those one 
write access key as long lifetime key.


Train model with dataset in which keys have at lest two writes.
msr prn_0  binary classification
metric: auc
```

[LightGBM] [Debug] Trained a tree with leaves = 31 and depth = 12
[LightGBM] [Debug] Trained a tree with leaves = 31 and depth = 10
Starting predicting...
              precision    recall  f1-score   support

       False       0.93      0.99      0.96    787787
        True       0.72      0.34      0.47     85914

    accuracy                           0.92    873701
   macro avg       0.83      0.66      0.71    873701
weighted avg       0.91      0.92      0.91    873701

0.8270840041098789
```
adjust class_weight to balanced , no change to results score 
metric: auc
```
[LightGBM] [Debug] Trained a tree with leaves = 31 and depth = 10
Starting predicting...
              precision    recall  f1-score   support

       False       0.93      0.99      0.96    787787
        True       0.72      0.34      0.47     85914

    accuracy                           0.92    873701
   macro avg       0.83      0.66      0.71    873701
weighted avg       0.91      0.92      0.91    873701

0.8270840041098789
```

Change scale_pos_weight to 9. precision drops.
```
    params = {
        'boosting_type': 'gbdt',
        'objective': 'binary',
        'metric': {'auc' },
        # 'class_weight': 'balanced',
        'scale_pos_weight': float(9),
        # 'class_weight': {0:9, 1:1},
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        # 'num_class': 4,
        'feature_fraction': 0.9,
        'verbose': 10

    }
Starting predicting...
              precision    recall  f1-score   support

       False       0.99      0.75      0.85    787787
        True       0.28      0.90      0.43     85914

    accuracy                           0.76    873701
   macro avg       0.63      0.82      0.64    873701
weighted avg       0.92      0.76      0.81    873701

0.6333894510161541
```


```
    params = {
        'boosting_type': 'gbdt',
        'objective': 'binary',
        'metric': {'binary_logloss' },
        # 'class_weight': 'balanced',
        'scale_pos_weight': float(9),
        # 'class_weight': {0:9, 1:1},
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        # 'num_class': 4,
        'feature_fraction': 0.9,
        'verbose': 10

    }
[LightGBM] [Debug] Trained a tree with leaves = 31 and depth = 14
Starting predicting...
         precision    recall  f1-score   support

       False       0.99      0.75      0.85    787787
        True       0.28      0.90      0.43     85914

    accuracy                           0.76    873701
   macro avg       0.63      0.82      0.64    873701
weighted avg       0.92      0.76      0.81    873701

0.6333894510161541
```

```
    params = {
        'boosting_type': 'gbdt',
        'objective': 'binary',
        'metric': {'binary_logloss' },
        # 'class_weight': 'balanced',
        # 'scale_pos_weight': float(9),
        # 'class_weight': {0:9, 1:1},
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        # 'num_class': 4,
        'feature_fraction': 0.9,
        'verbose': 10

    }

Starting predicting...
              precision    recall  f1-score   support

       False       0.93      0.99      0.96    787787
        True       0.72      0.34      0.47     85914

    accuracy                           0.92    873701
   macro avg       0.83      0.66      0.71    873701
weighted avg       0.91      0.92      0.91    873701

0.8270840041098789

```

Try combination of all meta params to train models.
Don't think I need to train with more params because 
amount of data is small.

I will try to add more features to training data.
And then I will train model. After that  I will integrate this 
model into rocksdb and see what is  wamp and space amp .


[Todo]
Chage train target to regression. 

mean square error of log lifetime label
```
Starting predicting...
mean_squared_error:  25.130439360393265
```

filter rows whose liftime is longer than 1e10
This does not really help. Let's integrate this model into rocksdb
and see what will happen.
```
mean_squared_error:  25.130439360393265
mean_squared_error orig:  3.516151516217961e+39
mean_squared_error orig filter:  8.663232997948649e+30
```
mean_filter_y_pred shows significant deviation from mean_filter_y_test.
Is there some bug in code that causes this?
I am not sure. 
I will integrate model into rocksdb now.
```
mean_squared_error:  3.8821685264259616
mean_squared_error orig:  416551.7144696365
mean_filter_y_test:  154525.92445634614
std_dev_filter_y_test:  391516.58894319227
mean_filter_y_pred:  3851.075198892032
std_dev_filter_y_pred:  16569.78845839742
mean_squared_error orig filter:  416551.7144696365
```
[Status: Done]

[Todo]
Add read delta and edcs to features. 
[Status: Not started]


Train model on full data including keys with one writw and at least two writes.
acc is lower for short term keys but higher for long term keys.
```
    params = {
        'boosting_type': 'gbdt',
        'objective': 'binary',
        'metric': {'auc' },
        'class_weight': 'balanced',
        # 'scale_pos_weight': float(9),
        # 'class_weight': {0:9, 1:1},
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        # 'num_class': 4,
        'feature_fraction': 0.9,
        'verbose': 10

    }

Starting predicting...
              precision    recall  f1-score   support

       False       0.90      0.90      0.90    787263
        True       0.61      0.62      0.62    209419

    accuracy                           0.84    996682
   macro avg       0.76      0.76      0.76    996682
weighted avg       0.84      0.84      0.84    996682

0.755314872887799
```


Want to set up k8s cluster but didn't make my time to do that.


[Todo]
Train binary model with pre_read_count extra feature
no improvement compared  to that without this this pre_read_count column.
Will add edcs and deltas of previous row with read to each write row.
```
    params = {
        'boosting_type': 'gbdt',
        'objective': 'binary',
        'metric': {'auc' },
        'class_weight': 'balanced',
        # 'scale_pos_weight': float(9),
        # 'class_weight': {0:9, 1:1},
        # 'metric': {'multi_logloss'},
        'num_leaves': 31,
        'learning_rate': 0.05,
        # 'num_class': 4,
        'feature_fraction': 0.9,
        'verbose': 10

    }

Starting predicting...
              precision    recall  f1-score   support

       False       0.90      0.90      0.90    787263
        True       0.61      0.62      0.62    209419

    accuracy                           0.84    996682
   macro avg       0.76      0.76      0.76    996682
weighted avg       0.84      0.84      0.84    996682

0.755533092738467
```
[Status: Done]


[Todo]
Test impact of number of features to model performance.
[Status: Not started]


[Todo]
 Add edcs and deltas of previous row with read to each write row.
[Status: Not started]



Adjust large_finite_number from 1e40 to 2 * larget_seen lifetime in dataset 
to reduce large number impact on the model training.

[Todo]
Add value size to training data.
[Status: Not started]


How many data does LRB use to train model and test? 
LRB trains model again after it accumulates 128k samples.

I need to design a way to label key lifetime as well. This will be contribution
of my paper as well


I need to train model on traces that can be replayed in one hour for testing now.
I will do ycsb or mixgraph now want to adjust the skewness of the distribution.

[Todo]
Set dist_a and dist_b params to adjust skewness of mixgraph workload 
and run workload.

Actually I can just run key gen function to see how key distribution is for
different key distribution params.
small value of dist_a and dist_b make key distribution less skewed.
I will set dist_a = dist_b = 0.1
```
byteorder: little
bytearray(b'\x00\x00\x00\x00\x00\x00\xdc\\aaaaaaaa'): 9988
bytearray(b'\x00\x00\x00\x00\x00\x00\xc1\taaaaaaaa'): 755
bytearray(b'\x00\x00\x00\x00\x00\x01b\xb6aaaaaaaa'): 453
bytearray(b'\x00\x00\x00\x00\x00\x01Gcaaaaaaaa'): 315
bytearray(b'\x00\x00\x00\x00\x00\x01,\x10aaaaaaaa'): 255
bytearray(b'\x00\x00\x00\x00\x00\x00G\x1daaaaaaaa'): 228
bytearray(b'\x00\x00\x00\x00\x00\x00+\xcaaaaaaaaa'): 191
bytearray(b'\x00\x00\x00\x00\x00\x00\x10waaaaaaaa'): 177
bytearray(b'\x00\x00\x00\x00\x00\x00\xb2$aaaaaaaa'): 155
bytearray(b'\x00\x00\x00\x00\x00\x00{~aaaaaaaa'): 121
bytearray(b'\x00\x00\x00\x00\x00\x00\x96\xd1aaaaaaaa'): 119
bytearray(b'\x00\x00\x00\x00\x00\x01\x01\xd8aaaaaaaa'): 117
bytearray(b'\x00\x00\x00\x00\x00\x00\xe6\x85aaaaaaaa'): 109
bytearray(b'\x00\x00\x00\x00\x00\x01\x1d+aaaaaaaa'): 108
bytearray(b'\x00\x00\x00\x00\x00\x01l\xdfaaaaaaaa'): 86
bytearray(b'\x00\x00\x00\x00\x00\x01Q\x8caaaaaaaa'): 79
bytearray(b'\x00\x00\x00\x00\x00\x00l\x99aaaaaaaa'): 75
bytearray(b'\x00\x00\x00\x00\x00\x00\x01\x92aaaaaaaa'): 71
bytearray(b'\x00\x00\x00\x00\x00\x00QFaaaaaaaa'): 68
bytearray(b'\x00\x00\x00\x00\x00\x00\xd7\xa0aaaaaaaa'): 68
bytearray(b'\x00\x00\x00\x00\x00\x00\xbcMaaaaaaaa'): 67
bytearray(b'\x00\x00\x00\x00\x00\x01B\xa7aaaaaaaa'): 64
```
[Status: Done]





[Todo]
Fix bug about free invalid pointer error when deleting merging iterator.
Free invalid pointer still occurs when we try to delete the internal iterator
immediately after it's created.

so we can track down the creation code to see what's wrong.
Turns out we should not call delete directly.
Memory of internal iterator is managed by Arena class. 
We should just call destructor of iterator.
[Status: Done]


[Todo]
Look for parallel apply in dask framework
```
ddf = dd.from_pandas(df, npartitions=2)
res = ddf.apply(myadd, axis=1, args=(2,), b=1.5, meta=('x', 'f8'))
```
Will try 30 npartitions
parallel apply is still slow compared to purememory computation in pandas
set num_workers=40 to have more threads
[Status: Done]


[Todo]
Get lifetime distribution for keys in mixgraph workload trace.
Use dask in k8s.
Have problem with dask framework.
This is so troublesome to use this framework.

Restart containerd service with aliyun mirror.
Steps
edit containerd config
```
sudo vim /etc/containerd/config.toml
update registry path to aliyun path.
```

Try run local cluster with simple script.
Then I can run complex script in loca cluster .
Can not connect to scheduler pod in k8s
I can do distributed computing in local cluster but cannot do that 
in k8s environment
Reaized that I can just write data to file instead of returning data after 
applying. Now my code doesn't use all memory on the system.
But again, I can't start dask k8s cluster to submit job.
It takes 10 hours to finish feature generation script
Why is it so long ? 
```

real    619m37.584s
user    617m47.719s
sys     2m13.993s
```
[Status: Done]

[Todo]
Train model on mixgraph trace and see the results.
number of keys in mixgraph 20M keys
```
➜  with_gc_1.0_0.8 cut -d ' ' -f 1,1  ./trace-human_readable_trace.txt | sort | uniq -c | wc -l
7561374
```
Binary classification results for data with at least two writes.
Recall rate is super low. This is not good.
```
Starting predicting...
              precision    recall  f1-score   support

       False       0.79      1.00      0.88    958223
        True       0.54      0.02      0.03    251970

    accuracy                           0.79   1210193
   macro avg       0.67      0.51      0.46   1210193
weighted avg       0.74      0.79      0.71   1210193

0.6659587378395888
```
Num of keys:  1872849 
Num of write: 6050963

Precision score increases to 0.88 after adding 
keys with one write .
Lots of keys are written only once.
Num of keys: 3981727  
Num of writes: 10001654
This is weird. Num of writes is 4M more than that 
in data with at least two writes.
This is correct because it's not that keys in at least 
two write file contains all writes of those keys.
I discard the last writes of those keys
```
              precision    recall  f1-score   support

       False       0.96      0.69      0.81    847793
        True       0.81      0.98      0.89   1152538

    accuracy                           0.86   2000331
   macro avg       0.89      0.84      0.85   2000331
weighted avg       0.88      0.86      0.85   2000331

0.8868538795333816
```


[Status: Done]


[Todo]
reply trace of mixgraph and test model performance
check replay to see how rocksdb convert hex format key to slice format key
I think it calls hextostring() to achieve that.
One problem occurs that how can I store past data and get features such 
as edcs and deltas. How can I calculate features in the flight ?
One naive solution is to store all data in memory and calculate them 
once we do flush.
How does lrb solve this problem?
I think we can allocate some amount of memory and do sampling to 
accumulate traning data. 
This is similar to time-series lsm-tree engine looks like.
We are making assumptions like short lifetime keys have similar 
edcs and deltas.
It's hard to directly load file into memory can index edcs for each key 
because you need to iterating and index key from the file.

I can load the feature file into memory and get feature related to key
from file with key and seq number.


[Status: Done]


[Todo]
Submit pr to fix the hextostring bug in rocksdb 
[Status: Not started]

[Todo]
Add flags environment variable to db_bench so that 
model path can be passed by shell running.
[Statsu: Done]


[Todo]
Find out where are booster_handle is set in db_options so that it 
is passed to compaciton iterator in compaction job.
I don't set booster_handle in db_options because I don't use 
that model in compaction job. 

Set booster_handle in flush_job

db_impl_compaction_flush.cc
```

    flush_job.SetBoosterHandleAndConfig(lightgbm_handle_, this->lightgbm_fastConfig_);
```

flush_job.cc
```
          job_context_->GetJobSnapshotSequence();
      tboptions.compaction_tracer = compaction_tracer_;
      tboptions.booster_handle = booster_handle_;
      tboptions.lifetime_bucket_num = db_options_.num_classification;
      tboptions.booster_fast_config_handle = fast_config_handle_;
      tboptions.cfd = cfd_;
      s = BuildTable(
```
[Status: Done]



[Todo]
Figure out replay code logcis and see if hex key is used in replay
replayer_impl.cc:117
```


        if (options.num_threads <= 1) {
    // num_threads == 0 or num_threads == 1 uses single thread.
    std::chrono::system_clock::time_point replay_epoch =
        std::chrono::system_clock::now();

    while (s.ok()) {
      Trace trace;
      s = ReadTrace(&trace);
      // If already at trace end, ReadTrace should return Status::Incomplete().
      if (!s.ok()) {
        break;
      }


```

trace_replay.cc: 393
```
Status Tracer::WriteWithStartSequence(WriteBatch *write_batch, uint64_t start_seq, uint64_t write_rate) {
  TraceType trace_type = kTraceWriteWithStartSequence;
  if (ShouldSkipTrace(trace_type)) {
    return Status::OK();
  }
  Trace trace;
  trace.ts = clock_->NowMicros();
  trace.type = trace_type;
  TracerHelper::SetPayloadMap(trace.payload_map,
                                  TracePayloadType::kWriteBatchStartSequence);
  PutFixed64(&trace.payload, trace.payload_map);
  PutFixed64(&trace.payload, start_seq);
  PutFixed64(&trace.payload, write_rate);
  // fprintf(stdout, "write rate is %lu\n", write_rate);
  PutLengthPrefixedSlice(&trace.payload, Slice(write_batch->Data()));

  return WriteTrace(trace);
}
```

```

Status ReplayerImpl::ReadTrace(Trace* trace) {
  return TracerHelper::DecodeTrace(encoded_trace, trace); // ts, type, payload

Status TracerHelper::DecodeTraceRecord(Trace* trace, int trace_file_version,
        GetFixed64(&buf, &trace->payload_map);
        GetFixed64(&buf, &start_sequence);
        GetFixed64(&buf, &write_rate_mb_per_sec);
        int64_t payload_map = static_cast<int64_t>(trace->payload_map);
        Slice write_batch_data;
        while(payload_map) {
          uint32_t set_pos = static_cast<uint32_t>(log2(payload_map & -payload_map));
          switch(set_pos) {
            case TracePayloadType::kWriteBatchStartSequence: {
              GetLengthPrefixedSlice(&buf, &write_batch_data);
 
```


So trace is written into trace file in ascii format.
hex format is written to human-readable trace file
[Status: Done]



[Todo]
Fix seq number writing bug.
This means that I need to regenerate features data  file.
Maybe I can just current file and get the key whose key is close
to the one used in compaction process.
ToString(hex=true)
```
// Return a string that contains the copy of the referenced data.
std::string Slice::ToString(bool hex) const {
  std::string result;  // RVO/NRVO/move
  if (hex) {
    result.reserve(2 * size_);
    for (size_t i = 0; i < size_; ++i) {
      unsigned char c = data_[i];
      result.push_back(toHex(c >> 4));
      result.push_back(toHex(c & 0xf));
    }
    return result;
  } else {
    result.assign(data_, size_);
    return result;
  }
}
```
stringtohex
```
std::string LDBCommand::StringToHex(const std::string& str) {
  std::string result("0x");
  result.append(Slice(str).ToString(true));
  return result;
}


```

Turns out that I use the wrong trace file to replay.
[Status: Done]


[Todo]
Where is ttl is defined?
Need to update this as well.
It's in compaction_iterator.cc
```
namespace ROCKSDB_NAMESPACE {
const  std::unordered_map<uint64_t, uint64_t> LifetimeLabelToSecMap ={
  {0, 281},
  {1, 1000}
};
// const  std::unordered_map<uint64_t, uint64_t> LifetimeLabelToSecMap ={
//   {0, 1},
//   {1, 10},
//   {2, 100},
//   {3, 1000}
// };

// const std::vector<uint64_t> LifetimeSecs = { 1, 10 , 100, 1000};
const std::vector<uint64_t> LifetimeSecs = { 281, 1000};


```
[Status: Done]


[Todo]
with_model
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    version     job_id  githash
0               158MB   4GB     7.7     1.8     4.1     1871    1759    33      6       1778574766.01918    0.0     0       2.9     0.2     64.0    replay.t1.s1    2024-01-1T15:46:37      8.0.0      1aacaadf6a       0.5
```
no_model 1.0_0.8
```

ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    version     job_id  githash
11245   1.2     158MB   4GB     28.1    4.4     16.2    804     802     21      27      88.9    140.9       284     536     2506    57160643251     1778    0.0     0       1.7     0.2     1.7     mixgraph.t1.s1      2023-12-20T17:37:09     8.0.0           fde21a6fe3      0.6

Uptime(secs): 1777.9 total, 1.0 interval
Cumulative writes: 9997K writes, 9997K keys, 9997K commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 4.54 MB/s
Cumulative compaction: 28.10 GB write, 16.20 MB/s write, 21.84 GB read, 12.59 MB/s read, 804.2 seconds


** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      2/0    6.58 MB   0.5      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     81.7     79.92             79.54        82    0.975       0      0       0.0       6.1
  L1      9/1   26.43 MB   0.9     14.4     0.3      0.4       0.6      0.2       0.0   1.0     31.0     30.9    476.06            474.84        20   23.803     18M  1514K      13.8      13.8
  L2     45/3   125.06 MB   0.6      7.4     0.1      0.2       0.3      0.1       0.0   1.0     30.7     30.5    248.21            247.59        39    6.364     10M  1592K       7.1       7.1
 Sum     56/4   158.07 MB   0.0     21.8     0.4      0.6       1.1      0.5       0.0   4.4     27.8     35.8    804.19            801.97       141    5.704     29M  3107K      20.8      27.0
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0


```

with_model with adjusted features num(inf -> uint64_t:max) and adjusted ttl for feach blob file(50->281, 100->1000)
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    version     job_id  githash
0               159MB   4GB     7.8     1.8     4.2     1818    1731    34      6       1778590275.01914    0.0     0       2.8     0.2     64.1    replay.t1.s1    2024-01-1T21:54:22      8.0.0      1aacaadf6a       0.5

Uptime(secs): 1913.2 total, 8.0 interval
Cumulative writes: 10M writes, 10M keys, 10M commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 4.22 MB/s
Cumulative compaction: 7.76 GB write, 4.16 MB/s write, 35.41 GB read, 18.95 MB/s read, 1817.5 seconds
```

with_model with adjusted features num(inf -> uint64_t:max) and adjusted ttl for feach blob file(50->281, 100->1000)
+ adjusted feature value(inf->zero)
```
Completed replay (ID: ) in 8230 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50     p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date      version job_id  githash
0               158MB   4GB     7.9     1.8     4.3     1804    1801    38      6       1778525570.0                                            1901    0.0     0       2.9     0.2     6.4     replay.t1.s1      2024-01-3T15:34:39      8.0.0           1aacaadf6a      0.5




** DB Stats **
Uptime(secs): 1893.1 total, 1.0 interval
Cumulative writes: 9999K writes, 9999K keys, 9999K commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 4.26 MB/s
Cumulative WAL: 9999K writes, 0 syncs, 9999618.00 writes per sync, written: 7.88 GB, 4.26 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 5591 writes, 5592 keys, 5591 commit groups, 1.0 writes per commit group, ingest: 4.51 MB, 4.51 MB/s
Interval WAL: 5591 writes, 0 syncs, 5591.00 writes per sync, written: 0.00 GB, 4.51 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.7    242.11            241.61        85    2.848       0      0       0.0       6.1
  L1     10/1   29.17 MB   1.1     15.0     0.3      0.4       0.6      0.2       0.0   1.0     29.6     29.5    519.20            518.32        21   24.724     19M  1533K      14.3      14.3
  L2     46/3   124.44 MB   0.6      7.9     0.1      0.2       0.3      0.1       0.0   1.0     29.8     29.7    269.77            269.28        55    4.905     11M  1550K       7.5       7.5
 Sum     57/4   156.76 MB   0.0     22.9     0.4      0.6       1.2      0.5       0.0   4.6     22.7     28.9   1031.08           1029.20       161    6.404     30M  3084K      21.8      27.9
 Int      0/0    0.00 KB   0.0      0.2     0.0      0.0       0.0      0.0       0.0 188865559.0     29.4     29.2      6.17              6.16         1    6.173    259K    45K       0.2       0.2


```
b_rgb doesn't change and is even worse after I update inf to 0.
lifetime label: 1 blob files have more number of keys than original.
There are 420 jobs during whole replay process which is more than that 
in standard rocksdb.
Standard rocksdb have 146 jobs.
It should be time calculation problem.
Let me check that.
```
Completed replay (ID: ) in 2020 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    version     job_id  githash
0               157MB   4GB     7.2     1.7     3.9     1023    1021    22      6       1778525335.01892    0.0     0       2.3     0.1     5.8     replay.t1.s1    2024-01-4T12:10:05      8.0.0      1aacaadf6a       0.5


** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.6    243.75            243.25        85    2.868       0      0       0.0       6.1
  L1      9/1   29.79 MB   1.1     14.9     0.3      0.4       0.6      0.2       0.0   0.0     29.8      1.2    511.07            510.16        21   24.337     19M  1522K      14.2       0.0
  L2     46/3   124.10 MB   0.6      7.8     0.1      0.2       0.3      0.1       0.0   0.0     29.8      1.2    268.37            267.91        51    5.262     10M  1554K       7.4       0.0
 Sum     56/4   157.04 MB   0.0     22.7     0.4      0.6       1.2      0.5       0.0   1.1     22.7      7.2   1023.19           1021.32       157    6.517     30M  3076K      21.6       6.1
 Int      0/0    0.00 KB   0.0      0.2     0.0      0.0       0.0      0.0       0.0 6578094.0     29.3      1.2      5.31              5.29         1    5.306    226K    44K       0.1       0.0


```
167 jobs in this run which is close to no model..
8 ForcedBlobGC jobs.
But there is no ForcedBlobGC jobs in no model. I don't know hwy w_amp is 4.4 in 
no model runnning.
Is this because of some code changes
stats meaning for each short key
https://github.com/facebook/rocksdb/wiki/Compaction-Stats-and-DB-Status
I don't know hwat Int mean.

```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs       c_csecs b_rgb   b_wgb   usec_op p50     p99     p99.9p99.99   pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss  test     date    version job_id  githash
0               157MB   4GB     29.1    4.5     15.7    1031 1029     22      28      1778525351.0                         1894     0.0     0       2.3     0.1     5.8     replay.t1.s1 2024-01-5T10:23:00       8.0.0           1aacaadf6a      0.5





** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.7    242.11            241.61        85    2.848       0      0       0.0       6.1
  L1     10/1   29.17 MB   1.1     15.0     0.3      0.4       0.6      0.2       0.0   1.0     29.6     29.5    519.20            518.32        21   24.724     19M  1533K      14.3      14.3
  L2     46/3   124.44 MB   0.6      7.9     0.1      0.2       0.3      0.1       0.0   1.0     29.8     29.7    269.77            269.28        55    4.905     11M  1550K       7.5       7.5
 Sum     57/4   156.76 MB   0.0     22.9     0.4      0.6       1.2      0.5       0.0   4.6     22.7     28.9   1031.08           1029.20       161    6.404     30M  3084K      21.8      27.9
 Int      0/0    0.00 KB   0.0      0.2     0.0      0.0       0.0      0.0       0.0 188865559.0     29.4     29.2      6.17              6.16         1    6.173    259K    45K       0.2       0.2

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0     22.9     0.4      0.6       0.9      0.3       0.0   0.0     29.7     29.5    788.97            787.60        76   10.381     30M  3084K      21.8      21.8
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.7    242.11            241.61        85    2.848       0      0       0.0       6.1


```

Update gc logic in compaction_iterator so that values whose lifetime label doesn't change
won't trigger rewrite of blob .
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50    p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    versionjob_id   githash
0               153MB   6GB     11.5    2.3     6.2     1356    1354    1       7       1778525364.0   1894     0.0     0       2.6     0.1     5.9     replay.t1.s1    2024-01-6T12:49:32      8.0.0          1aacaadf6a       0.8




** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.3    246.70            246.22        85    2.902       0      0       0.0       6.1
  L1      8/0   23.63 MB   0.9      0.7     0.3      0.4       0.6      0.2       0.0   2.3      3.4      3.1    196.60            196.41        21    9.362     19M  1522K       0.0       0.0
  L2     46/1   126.26 MB   0.6      4.6     0.1      3.3       3.4      0.1       0.0   3.5      5.2      5.1    911.72            910.53      1147    0.795    102M  1658K       1.2       1.2
 Sum     55/1   153.04 MB   0.0      5.3     0.4      3.7       4.2      0.5       0.0   1.8      4.0      8.7   1355.02           1353.17      1253    1.081    122M  3181K       1.2       7.2
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0     -0.0       0.0 33580123.0      4.2      4.2      7.57              7.55        19    0.398    888K      0       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      5.3     0.4      3.7       4.0      0.3       0.0   0.0      4.9      4.8   1108.32           1106.94      1168    0.949    122M  3181K       1.2       1.2
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.3    246.70            246.22        85    2.902       0      0       0.0       6.1

Blob file count: 358, total size: 6.1 GB, garbage size: 2.6 GB, space amp: 1.7


```
Key_In increases 170M(I think this is caused by too many forcedblobgc jobs) , Read drops 17G , No big difference in key_drop..Blob sz increases 2GB.
1247 jobs which is too many. We should not see ForcedBlobGC compaction_reason.
See 1106 ForcedBlobGC jobs. This is because rocksdb think there are too mnay garbage in the blob 
.
We will have all blob files classified as short if we don't run model for 
them during compaction jobs. 
```
2024/01/06-13:15:27.822263 3884117 [db/compaction/compaction_job.cc:1666] [default] [JOB 1020] Generated table #1633: 94627 keys, 3406468 bytes, temperature: kUnknown

2024/01/06-13:15:28.632358 3884117 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 1021] Delete /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/001633.sst type=2 #1633 -- OK
```
1633.sst is generated in job 1020 but is immediately deleted in job 1021 one sec later which should not happen .
I think there is no creation timestamp for for new blob during forcedblobgc compaction which cause this frequent gc jobs.
Will update blob file addition creation constructor.
It's also because that I don't rewrite blobs to new blob files which causes old garbage file to be recyled over and over again.
I can add code to force write blob to new blob file if compaction reason is forcedblbogc to avoid gc happens for same file 
over and over again. This way we can differentiate between compaction and real gc jobs.

No enable_blob_garbage_collection in ForcedBlobGC compaction job which is not right. Need to check why this happens.

```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50    p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    versionjob_id   githash
0               152MB   6GB     11.1    2.2     6.0     660     659     4       10      1778525353.0   1892     0.0     0       2.0     0.1     5.8     replay.t1.s1    2024-01-6T22:04:13      8.0.0          1aacaadf6a       0.8


** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.6    243.04            242.67        85    2.859       0      0       0.0       6.1
  L1      7/0   20.95 MB   0.8      0.7     0.3      0.4       0.6      0.2       0.0   2.1      3.8      3.5    197.02            196.85        22    8.956     19M  1519K       0.1       0.1
  L2     50/1   127.64 MB   0.6      4.2     0.1      0.4       0.5      0.1       0.0   1.1     19.6     19.3    219.48            219.09        90    2.439     15M  1706K       3.7       3.7
 Sum     58/1   151.74 MB   0.0      4.9     0.4      0.8       1.3      0.6       0.0   1.8      7.6     17.3    659.54            658.60       197    3.348     34M  3225K       3.8       9.8
 Int      0/0    0.00 KB   0.0      0.3     0.0      0.0       0.0     -0.0       0.0 294797532.0     32.7     32.7      8.59              8.56         5    1.718    333K      0       0.3       0.3

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      4.9     0.4      0.8       1.1      0.3       0.0   0.0     12.1     11.8    416.50            415.94       112    3.719     34M  3225K       3.8       3.8
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.6    243.04            242.67        85    2.859       0      0       0.0       6.1

Blob file count: 172, total size: 5.9 GB, garbage size: 2.4 GB, space amp: 1.7

```
Key_in is the same as no_model which is 30M. w_amp is low. space_amp is high.
We have 6GB at the end.
```
no_model   r_gb: 20.8     w_gb: 27.0
with_model r_gb:3.8       w_gb:9.8

```

update gc sst files selection logics. No decrease in blob_sz
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50    p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    versionjob_id   githash
0               153MB   6GB     11.3    2.2     6.1     664     662     4       10      1778525982.0   1893     0.0     0       2.0     0.1     5.8     replay.t1.s1    2024-01-7T12:22:14      8.0.0          1aacaadf6a       0.8
```

```

2024/01/07-12:51:30.635997 1859860 [db/compaction/compaction_job.cc:1666] [default] [JOB 204] Generated table #832: 41354 keys, 1493183 bytes, temperature: kUnknown
2024/01/07-12:51:36.508700 1859860 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000832.sst immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
```


```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50    p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    versionjob_id   githash
0               152MB   6GB     11.1    2.2     6.0     657     656     4       10      1778525357.0   1892     0.0     0       2.0     0.1     5.8     replay.t1.s1    2024-01-7T17:05:13      8.0.0          1aacaadf6a       0.8

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.9    241.09            240.75        85    2.836       0      0       0.0       6.1
  L1      7/0   21.46 MB   0.9      0.7     0.3      0.4       0.6      0.2       0.0   2.1      3.8      3.5    196.28            196.11        22    8.922     19M  1525K       0.1       0.1
  L2     50/1   127.38 MB   0.6      4.1     0.1      0.4       0.4      0.1       0.0   1.1     19.5     19.2    217.25            216.86        91    2.387     15M  1693K       3.6       3.6
 Sum     58/1   151.98 MB   0.0      4.9     0.4      0.8       1.3      0.6       0.0   1.8      7.6     17.3    654.61            653.72       198    3.306     34M  3218K       3.7       9.8
 Int      0/0    0.00 KB   0.0      0.2     0.0      0.0       0.0     -0.0       0.0 250092158.0     32.4     32.4      7.36              7.33         4    1.840    282K      0       0.2       0.2

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      4.9     0.4      0.8       1.1      0.3       0.0   0.0     12.0     11.8    413.52            412.96       113    3.659     34M  3218K       3.7       3.7
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.9    241.09            240.75        85    2.836       0      0       0.0       6.1

Blob file count: 175, total size: 6.0 GB, garbage size: 2.5 GB, space amp: 1.7


```

There are more b_rgb than that in standard rocksdb ? why is that?
b_wgb is much lower than that in standard rocksdb . why is that ? 
c_mbps is very low. Is this because we need get features for each key?
This might be the problem.
mixgraph dataset analysis
More than 50% of keys should be labelled as long. This is not the case from what
we see in log.
```
True     5755504
False    4246148
```

[Todo]
Need to update conversion code to turn 'inf' to a inf in uint64_t max
Currently there is very low number of keys are labelled as long.
The expriment shows that there are very low number of keys are labelled as long 
lifetime which is what we expect.
[Status: Done]


[Todo]
Select blob files to do compaction , and then do a read during garbage collection
process to test the validity of the valud and decide if the value should be
written back to rocksdb or not.
[Status: Not started]

[Todo]
Train log time model 
[Status: Not started]

[Todo]
Use log time model to predict lifetime of keys.
Need to dynamically create lifetime classification blob files.
[Status: Not started]

Plot lifetime graph wrong, replot and get a graph no so skewed.
I don't know how to set categories.
I previsouly set 281 as short lifetime. I think it's ok.
I don't know how to transform inf value to train model.
How does lrb achieve that?

[Todo]
Learn how preparation of data for missing value is done in lrb.
```
void LRBCache::train() {
    ++n_retrain;
    auto timeBegin = chrono::system_clock::now();
    if (booster) LGBM_BoosterFree(booster);
    // create training dataset
    DatasetHandle trainData;
    LGBM_DatasetCreateFromCSR(
            static_cast<void *>(training_data->indptr.data()),
            C_API_DTYPE_INT32,
            training_data->indices.data(),
            static_cast<void *>(training_data->data.data()),
            C_API_DTYPE_FLOAT64,
            training_data->indptr.size(),
            training_data->data.size(),
            n_feature,  //remove future t
            training_params,
            nullptr,
            &trainData);

    LGBM_DatasetSetField(trainData,
                         "label",
                         static_cast<void *>(training_data->labels.data()),
                         training_data->labels.size(),
                         C_API_DTYPE_FLOAT32);


```

CSR represents compressed sparsed row 
```
Compressed Sparse Row (CSR): In the context of sparse matrices, the CSR 
or Compressed Row Storage (CRS) or Yale format represents a matrix by three 
(one-dimensional) arrays, that respectively contain nonzero values, 
the extents of rows, and column indices3.


import numpy as np
from scipy.sparse import csr_matrix

# Define the row, column, and data arrays
row = np.array([0, 0, 1, 2, 2, 2])
col = np.array([0, 2, 2, 0, 1, 2])
data = np.array([1, 2, 3, 4, 5, 6])

# Create the CSR matrix
matrix = csr_matrix((data, (row, col)), shape=(3, 3))

# Convert the CSR matrix to a dense array for display
dense_matrix = matrix.toarray()

print(dense_matrix)

In the context of a Compressed Sparse Row (CSR) matrix, `row`, `col`, and `data` are arrays that represent the structure of the sparse matrix:

- `row`: This is an array of **row indices** for each non-zero element in the matrix. It indicates the row location for each non-zero element.

- `col`: This is an array of **column indices** for each non-zero element in the matrix. It indicates the column location for each non-zero element.

- `data`: This is an array that contains the **values** of the non-zero elements in the matrix.

Together, these three arrays allow the CSR format to represent a sparse matrix in a compact way, only storing the non-zero elements and their locations. This is particularly useful for large matrices where most of the elements are zero.
```

So inf value in my feature generated dataset is represented as zero in lrb dataset.
Does this make sense?
The papser says that delta_i which does not exist is represented as inf . This is a conflict.


model training with csr format code example
```
void HypotheticalModel::train(const CSRMatrix& data) {
    // Initialize weights randomly
    std::vector<double> weights(data.num_cols(), 0.0);

    // Set learning rate and number of epochs
    double learning_rate = 0.01;
    int epochs = 100;

    // SGD loop
    for (int epoch = 0; epoch < epochs; ++epoch) {
        for (int i = 0; i < data.num_rows(); ++i) {
            // Calculate prediction
            double prediction = 0.0;
            for (int idx = data.row_ptr(i); idx < data.row_ptr(i + 1); ++idx) {
                prediction += weights[data.col_idx(idx)] * data.values(idx);
            }

            // Calculate error
            double error = target[i] - prediction; // Assume target is some existing vector

            // Update weights
            for (int idx = data.row_ptr(i); idx < data.row_ptr(i + 1); ++idx) {
                weights[data.col_idx(idx)] += learning_rate * error * data.values(idx);
            }
        }
    }

    // Save the trained weights to the model
    this->weights = weights;
}

```
[Status: Done]


[Todo]
Do batch prediction instead of calling predict for each key
Consider to use CSR data format for our dataset as well.
[Status: Not started]


Retrain model by replacing inf value with zero and get the same predictions results 
as before.
```
              precision    recall  f1-score   support

       False       0.96      0.69      0.81    847793
        True       0.81      0.98      0.89   1152538

    accuracy                           0.86   2000331
   macro avg       0.89      0.84      0.85   2000331
weighted avg       0.88      0.86      0.85   2000331

0.8869472988081286
```


[Todo]
Check ttl calculation code and fix the bug if possible to 
avoid too many forcedblobgc job.

log code 
```
    ROCKS_LOG_INFO(cfd->ioptions()->info_log.get(),
                   "[%s]CF has reserved epoch number %" PRIu64
                   " for files ingested "
                   "behind since `Options::allow_ingest_behind` is true",
                   cfd->GetName().c_str(), reserved_epoch_number);

  assert(immutable_options_);
  ROCKS_LOG_INFO(immutable_options_->logger,
                 "[%s] [JOB %d] Generated blob file #%" PRIu64 ": %" PRIu64
                 " total blobs, %" PRIu64 " total bytes, lifetime label: %" PRIu64  ,
                 column_family_name_.c_str(), job_id_, blob_file_number,
                 blob_count_, blob_bytes_, lifetime_label_);

```
blob_file_creation_time_sec is 0 which is not right. 
There should be problems when we pass creation timestamp to 
blob_file_additions.
```
(gdb) p elapsed_sec
$5 = 1704288768
(gdb) p now_sec
$6 = 1704288768
(gdb) p blob_file_creation_time_sec
$7 = 0
```
[Status: Abandoned]



[Todo]
Figure out how now micros is calculated

```
class SystemClock : public Customizable {

```

```
class CompositeEnv : public Env {
 public:
  // Initialize a CompositeEnvWrapper that delegates all thread/time related
  // calls to env, and all file operations to fs
  explicit CompositeEnv(const std::shared_ptr<FileSystem>& fs,
                        const std::shared_ptr<SystemClock>& clock)
      : Env(fs, clock) {}


```
Env is composite of fs and systemclock.
This is why I don't see definition of nowmicros in env_posix.cc file.
It should return correct micros tiemstamp.
Is this because of construction issue? I don't pass teimstamp to blob file builder?
Found the root cause of timestamp =0.
I don't pass timestamp to blob file builder in compaction job. 
```

  if(enable_blob_file_builder ) {
    for(size_t i =0; i < blob_file_builders.size(); i++){
      uint64_t timestamp = env_->NowMicros();
      blob_file_builders[i] = std::unique_ptr<BlobFileBuilder>(
      new BlobFileBuilder(
        versions_, fs_.get(),
        sub_compact->compaction->immutable_options(),
        mutable_cf_options, &file_options_, db_id_, db_session_id_,
        job_id_, cfd->GetID(), cfd->GetName(), Env::IOPriority::IO_LOW,
        write_hint_, io_tracer_, blob_callback_,
        BlobFileCreationReason::kCompaction, &blob_file_paths,
        sub_compact->Current().GetBlobFileAdditionsPtr(),
          i, timestamp));

        blob_file_builders_raw[i] = blob_file_builders[i].get();
    }
  }


```
[Status: Done]


[Todo]
Figure out how w_amp is calculated why there is difference between no_model
and with_model db_bench run.
definition of w_amp
https://github.com/facebook/rocksdb/wiki/Compaction-Stats-and-DB-Status
```
W-Amp: (total bytes written to level N+1) / (total bytes read from level N). This is the write amplification from compaction between levels N and N+1
```
I see, there is problem with stats update. I might remove the code or don't update stats for blob write in my current code.

internal_stats.cc
where is w_amp defined. 
Need to check bytes_written and bytes_written_blob in compaction job.
Do we need to check flush job ?
```
  double w_amp =
      (interval_stats.bytes_written + interval_stats.bytes_written_blob) /
      static_cast<double>(interval_ingest);
  PrintLevelStats(buf, sizeof(buf), "Int", 0, 0, 0, 0, w_amp, interval_stats);
```

similar to that of internal stats.
```
  if (level == 0) {
        input_bytes = curr_ingest;
      } else {
        input_bytes = comp_stats_[level].bytes_read_non_output_levels +
                      comp_stats_[level].bytes_read_blob;
      }
      double w_amp =
          (input_bytes == 0)
              ? 0.0
              : static_cast<double>(comp_stats_[level].bytes_written +
                                    comp_stats_[level].bytes_written_blob) /
                    input_bytes;
      std::map<LevelStatType, double> level_stats;
      PrepareLevelStats(&level_stats, files, files_being_compacted[level],
                        static_cast<double>(vstorage->NumLevelBytes(level)),
                        compaction_score[level], w_amp, comp_stats_[level]);
      (*levels_stats)[level] = level_stats;
    }
```
I will check bytes_written and bytes_written_blob in version_set.cc, version_builder.cc and flush_job.cc, compaction_job.cc
and blob_file_builder.cc
0 blob output in log which prove my theory.
```
2024/01/04-12:31:21.174296 3684609 (Original Log Time 2024/01/04-12:31:21.174077) [db/compaction/compaction_job.cc:867] [default] compacted to: files[1 9 36 0 0 0 0 0] max score 1.13, MB/sec: 30.9 rd, 1.2 wr, level 2, files in(1, 3) out(3 +0 blob) MB in(4.4, 6.7 +217.6 blob) out(9.3 +0.0 blob), read-write-amplify(1.1) write-amplify(0.0) OK, records in: 323421, records dropped: 54693 output_compression: NoCompression
```

Method in internal_stats to update bytes_written
```
  compact_->compaction->SetOutputTableProperties(std::move(tp));

  // Finish up all book-keeping to unify the subcompaction results
  compact_->AggregateCompactionStats(compaction_stats_, *compaction_job_stats_);
  UpdateCompactionStats();

 
    void CompactionState::AggregateCompactionStats(
        InternalStats::CompactionStatsFull& compaction_stats,
        CompactionJobStats& compaction_job_stats) {
      for (const auto& sc : sub_compact_states) {
        sc.AggregateCompactionStats(compaction_stats);
        compaction_job_stats.Add(sc.compaction_job_stats);
      }
    }

        void Add(const CompactionOutputsStats& stats) {
              this->num_output_files += static_cast<int>(stats.num_output_files);
              this->num_output_records += stats.num_output_records;
              this->bytes_written += stats.bytes_written;
              this->bytes_written_blob += stats.bytes_written_blob;
              this->num_output_files_blob +=
                  static_cast<int>(stats.num_output_files_blob);
            }
```


compactino_iterator has stats as well.
```
auto c_iter = std::make_unique<CompactionIterator>(
      input, cfd->user_comparator(), &merge, versions_->LastSequence(),
      &existing_snapshots_, earliest_write_conflict_snapshot_, job_snapshot_seq,
      snapshot_checker_, env_, ShouldReportDetailedTime(env_, stats_),
      /*expect_valid_internal_key=*/true, range_del_agg.get(),
      blob_file_builders_raw, db_options_.allow_data_in_errors,
      db_options_.enforce_single_del_contracts, manual_compaction_canceled_,
      db_options_.booster_handle,
      db_options_.booster_fast_config_handle,
      cfd,
      sub_compact->compaction, compaction_filter, shutting_down_,
      db_options_.info_log, full_history_ts_low, preserve_time_min_seqno_,
      preclude_last_level_min_seqno_);

while() {
    status = sub_compact->AddToOutput(*c_iter, open_file_func, close_file_func);
        return Current().AddToOutput(iter, open_file_func, close_file_func);
            AddToOutput() {
                if (!HasBuilder()) {
                            s = open_file_func(*this);
                            if (!s.ok()) {
                              return s;
                            }
                          }

            }
          

  assert(builder_ != nullptr);
  const Slice& value = c_iter.value();
  s = current_output().validator.Add(key, value);
  if (!s.ok()) {
    return s;
  }
  builder_->Add(key, value);

  stats_.num_output_records++;
  current_output_file_size_ = builder_->EstimatedFileSize();

 

    c_iter->Next()

}

// there is no bytes_written update in this function.
Status CompactionJob::FinishCompactionOutputFile(
    const Status& input_status, SubcompactionState* sub_compact,
    CompactionOutputs& outputs, const Slice& next_table_min_key) {
  AutoThreadOperationStageUpdater stage_updater(
      ThreadStatus::STAGE_COMPACTION_SYNC_FILE);
  assert(sub_compact != nullptr);
  assert(outputs.HasBuilder());

  FileMetaData* meta = outputs.GetMetaData();
  uint64_t output_number = meta->fd.GetNumber();
  assert(output_number != 0);

  ColumnFamilyData* cfd = sub_compact->compaction->column_family_data();
  std::string file_checksum = kUnknownFileChecksum;
  std::string file_checksum_func_name = kUnknownFileChecksumFuncName;

  // Check for iterator errors
  Status s = input_status;

  // Add range tombstones
  auto earliest_snapshot = kMaxSequenceNumber;
  if (existing_snapshots_.size() > 0) {
    earliest_snapshot = existing_snapshots_[0];
  }
  if (s.ok()) {
    CompactionIterationStats range_del_out_stats;
    // if the compaction supports per_key_placement, only output range dels to
    // the penultimate level.
    // Note: Use `bottommost_level_ = true` for both bottommost and
    // output_to_penultimate_level compaction here, as it's only used to decide
    // if range dels could be dropped.
    if (outputs.HasRangeDel()) {
      s = outputs.AddRangeDels(
          sub_compact->start.has_value() ? &(sub_compact->start.value())
                                         : nullptr,
          sub_compact->end.has_value() ? &(sub_compact->end.value()) : nullptr,
          range_del_out_stats, bottommost_level_, cfd->internal_comparator(),
          earliest_snapshot, next_table_min_key, full_history_ts_low_);
    }
    RecordDroppedKeys(range_del_out_stats, &sub_compact->compaction_job_stats);
    TEST_SYNC_POINT("CompactionJob::FinishCompactionOutputFile1");
  }

  const uint64_t current_entries = outputs.NumEntries();

  s = outputs.Finish(s, seqno_time_mapping_);


```


bytes_wirtten is updated in Finish() in 
This is really a mess. 
I spent long time figuring out how to update stats in current code.
```
Status CompactionOutputs::Finish(const Status& intput_status,
                                 const SeqnoToTimeMapping& seqno_time_mapping) {

  const uint64_t current_bytes = builder_->FileSize();
  if (s.ok()) {
    meta->fd.file_size = current_bytes;
    meta->marked_for_compaction = builder_->NeedCompact();
  }
  current_output().finished = true;
  stats_.bytes_written += current_bytes;
  stats_.num_output_files = outputs_.size();



}
```

In flush job, stats.bytes_written_blob is updated after BuildTable() which is in
WriteLevel0Table()

Find the place where original stats update for bytes_written_blob happens.
It happens after while loop of compaction iterator.
I commented it before and didn't put in the for loop I wrote to destroy blob_file_builders
in compaction_job..
[Status: Done]


[Todo]
Fix bytes_written_blob missing bug.
There might be concurrency issue with blob_file_additions_ vector.
No, we don't have that for now because only one blob file addition
is created and appended to the end of blob_file_additions_
during c_iter->Next() call.
```
  if(enable_blob_file_builder ) {
    for(size_t i =0; i < blob_file_builders.size(); i++){
      uint64_t timestamp = env_->NowMicros();
      blob_file_builders[i] = std::unique_ptr<BlobFileBuilder>(
      new BlobFileBuilder(
        versions_, fs_.get(),
        sub_compact->compaction->immutable_options(),
        mutable_cf_options, &file_options_, db_id_, db_session_id_,
        job_id_, cfd->GetID(), cfd->GetName(), Env::IOPriority::IO_LOW,
        write_hint_, io_tracer_, blob_callback_,
        BlobFileCreationReason::kCompaction, &blob_file_paths,
        sub_compact->Current().GetBlobFileAdditionsPtr(),
          i, timestamp));

        blob_file_builders_raw[i] = blob_file_builders[i].get();
    }
  }

```

Call this function at the end to update bytes_written_blob in stats_
```
  void UpdateBlobStats() {
    assert(!is_penultimate_level_);
    stats_.num_output_files_blob = blob_file_additions_.size();
    for (const auto& blob : blob_file_additions_) {
      stats_.bytes_written_blob += blob.GetTotalBlobBytes();
    }
  }
```

[Status: Done]



Why don't compaction process do a read to see if keys can be dropped during compactin process?
It needs to check the whole lsm-tree to decided if a key can be dropped.

[Todo ]
Check how orignal stackabledb works in terms of garbge collection.

```
  // Relocates valid blobs residing in the oldest blob files if garbage
  // collection is enabled. Relocated blobs are written to new blob files or
  // inlined in the LSM tree depending on the current settings (i.e.
  // enable_blob_files and min_blob_size). Should only be called for blob
  // references (kTypeBlobIndex).
  //
  // Note: the stacked BlobDB implementation's compaction filter based GC
  // algorithm is also called from here.
  void GarbageCollectBlobIfNeeded();
```


blob_compaction_filter.cc
```
CompactionFilter::BlobDecision BlobIndexCompactionFilterGC::PrepareBlobOutput(
    const Slice& key, const Slice& existing_value,
    std::string* new_value) const {
  assert(new_value);

  const BlobDBImpl* const blob_db_impl = context().blob_db_impl;
  (void)blob_db_impl;

  assert(blob_db_impl);
  assert(blob_db_impl->bdb_options_.enable_garbage_collection);

  BlobIndex blob_index;
  const Status s = blob_index.DecodeFrom(existing_value);
  if (!s.ok()) {
    gc_stats_.SetError();
    return BlobDecision::kCorruption;
  }

  if (blob_index.IsInlined()) {
    gc_stats_.AddBlob(blob_index.value().size());

    return BlobDecision::kKeep;
  }

  gc_stats_.AddBlob(blob_index.size());

  if (blob_index.HasTTL()) {
    return BlobDecision::kKeep;
  }

  if (blob_index.file_number() >= context_gc_.cutoff_file_number) {
    return BlobDecision::kKeep;
  }

  // Note: each compaction generates its own blob files, which, depending on the
  // workload, might result in many small blob files. The total number of files
  // is bounded though (determined by the number of compactions and the blob
  // file size option).
  if (!OpenNewBlobFileIfNeeded()) {
    gc_stats_.SetError();
    return BlobDecision::kIOError;
  }

  PinnableSlice blob;
  CompressionType compression_type = kNoCompression;
  std::string compression_output;
  if (!ReadBlobFromOldFile(key, blob_index, &blob, false, &compression_type)) {
    gc_stats_.SetError();
    return BlobDecision::kIOError;
  }

  // If the compression_type is changed, re-compress it with the new compression
  // type.
  if (compression_type != blob_db_impl->bdb_options_.compression) {
    if (compression_type != kNoCompression) {
      const Status status =
          blob_db_impl->DecompressSlice(blob, compression_type, &blob);
      if (!status.ok()) {
        gc_stats_.SetError();
        return BlobDecision::kCorruption;
      }
    }
    if (blob_db_impl->bdb_options_.compression != kNoCompression) {
      blob_db_impl->GetCompressedSlice(blob, &compression_output);
      blob = PinnableSlice(&compression_output);
      blob.PinSelf();
    }
  }

  uint64_t new_blob_file_number = 0;
  uint64_t new_blob_offset = 0;
  if (!WriteBlobToNewFile(key, blob, &new_blob_file_number, &new_blob_offset)) {
    gc_stats_.SetError();
    return BlobDecision::kIOError;
  }

  if (!CloseAndRegisterNewBlobFileIfNeeded()) {
    gc_stats_.SetError();
    return BlobDecision::kIOError;
  }

  BlobIndex::EncodeBlob(new_value, new_blob_file_number, new_blob_offset,
                        blob.size(), compression_type);

  gc_stats_.AddRelocatedBlob(blob_index.size());

  return BlobDecision::kChangeValue;
}
```
It's basicaly the same as blobDB garbage collection process.

[Status: Done]


HashKV avoids validity checks which means avoid read during GC which is good
for our work as well.
Can we come up with solution to group keys and lifetime together??

If we do validity check during compaction/garbage collection process there will
be no need for model.

Why do we see blob file deletion in level compaction between level_0 and level_1?
So this mean that all files in level0 are used in this compaction process so 
all blob files are deleted.

compaction_job.cc
In this function we see log compaction_reason
It's called in CompactionJob::Run()
```
Status CompactionJob::Run() {
  AutoThreadOperationStageUpdater stage_updater(
      ThreadStatus::STAGE_COMPACTION_RUN);
  TEST_SYNC_POINT("CompactionJob::Run():Start");
  log_buffer_->FlushBufferToLog();
  LogCompaction();


```
```
void CompactionJob::LogCompaction() {
  Compaction* compaction = compact_->compaction;
  ColumnFamilyData* cfd = compaction->column_family_data();

  // Let's check if anything will get logged. Don't prepare all the info if
  // we're not logging
  if (db_options_.info_log_level <= InfoLogLevel::INFO_LEVEL) {
    Compaction::InputLevelSummaryBuffer inputs_summary;
    ROCKS_LOG_INFO(
        db_options_.info_log, "[%s] [JOB %d] Compacting %s, score %.2f",
        cfd->GetName().c_str(), job_id_,
        compaction->InputLevelSummary(&inputs_summary), compaction->score());
    char scratch[2345];
    compaction->Summary(scratch, sizeof(scratch));
    ROCKS_LOG_INFO(db_options_.info_log, "[%s]: Compaction start summary: %s\n",
                   cfd->GetName().c_str(), scratch);
    // build event logger report
    auto stream = event_logger_->Log();
    stream << "job" << job_id_ << "event"
           << "compaction_started"
           << "compaction_reason"
           << GetCompactionReasonString(compaction->compaction_reason());
    for (size_t i = 0; i < compaction->num_input_levels(); ++i) {
      stream << ("files_L" + std::to_string(compaction->level(i)));
      stream.StartArray();
      for (auto f : *compaction->inputs(i)) {
        stream << f->fd.GetNumber();
      }
      stream.EndArray();

```

[Todo]
Implement wisckey garbage collection process.
Should we include both wisckey gc and rocksdb gc together? 
compaction_picker_level.cc
```
void LevelCompactionBuilder::SetupInitialFiles() {
  // Find the compactions by size on all levels.
  bool skipped_l0_to_base = false;
  for (int i = 0; i < compaction_picker_->NumberLevels() - 1; i++) {
    start_level_score_ = vstorage_->CompactionScore(i);

```
I can put new blob gc function in MaybeScheduleFlushOrCompaction();

Can no delete ExtractLargeValueIfNeededImpl() in compaction iterator 
to disable gc in normal compaction.


blob is updated if its blob num > age_cutoff_number
```
    if (blob_index.file_number() >=
        blob_garbage_collection_cutoff_file_number_) {
      return;
    }
```
Where is it defined?

```
uint64_t CompactionIterator::ComputeBlobGarbageCollectionCutoffFileNumber(
    const CompactionProxy* compaction) {
  if (!compaction) {
    return 0;
  }

  if (!compaction->enable_blob_garbage_collection()) {
    return 0;
  }

  const Version* const version = compaction->input_version();
  assert(version);

  const VersionStorageInfo* const storage_info = version->storage_info();
  assert(storage_info);

  const auto& blob_files = storage_info->GetBlobFiles();

  const size_t cutoff_index = static_cast<size_t>(
      compaction->blob_garbage_collection_age_cutoff() * blob_files.size());

  if (cutoff_index >= blob_files.size()) {
    return std::numeric_limits<uint64_t>::max();
  }

  const auto& meta = blob_files[cutoff_index];
  assert(meta);

  return meta->GetBlobFileNumber();
}
```
This is no good for our gc method. Need to change this.
What is 

Where is enable_blob_garbage_collection() is defined?
```
enable_blob_garbage_collection_(
          _blob_garbage_collection_policy == BlobGarbageCollectionPolicy::kForce
              ? true
              : (_blob_garbage_collection_policy ==
                         BlobGarbageCollectionPolicy::kDisable
                     ? false
                     : mutable_cf_options()->enable_blob_garbage_collection)),

```
Don't think we can see gc for blobs in normal rocksdb run because age_cutoff=1.0.
So I think enable_blob_garbage_collection_ is enable for every compaction.


Where is MaybeScheduleFlushOrCompaction() called and when?
```
MaybeScheduleFlushOrCompaction()
```

db_impl_compaction_flush.cc
Need to check where is unscheduled_compactions_ changed.
```

BackgroundCompaction()
    unscheduled_compactions_++

    auto cfd = PickCompactionFromQueue(&task_token, log_buffer); // 3252
    c.reset(cfd->PickCompaction(*mutable_cf_options, mutable_db_options_,
                                  log_buffer)); //3282
 
```

```

void DBImpl::SchedulePendingCompaction(ColumnFamilyData* cfd) {  or !enough_room or
cfd->NeedsCompaction() or compaction_failed

    void DBImpl::AddToCompactionQueue(ColumnFamilyData* cfd) {
      assert(!cfd->queued_for_compaction());
      cfd->Ref();
      compaction_queue_.push_back(cfd);
      cfd->set_queued_for_compaction(true);
    }
```
So cfd->NeedsCompaction() first, and then PickCompactionFromQueue() and then cfd->PickCompaction()
So I can add my logics to cfd->NeedsCompaction()

cfd->NeedsCompaction() check files_marked_for_forced_blob_gc, so there is 
a function call before NeedsCompaction() to put files to this vector.
It's called in version_set.cc ComputeFilesMarkedForForcedBlobGC(), when is 
it called? It's called in 
```
void VersionSet::AppendVersion(ColumnFamilyData* column_family_data,
                               Version* v) {
    VersionStorageInfo::ComputeCompactionScore()   
        ComputeFilesMarkedForForcedBlobGC(),
```
I can put blob files need to be gced into NeedsCompaction() function.
And then write a new function to set up gc job and run instead of 
creating compaction job ..

Traditional compaction jobs need to call InstallSuperVersionAndScheduleWork()
But my GC jobs need not to do that .
How can I create GC job struct from PickCompaction() function?

```

void DBImpl::MaybeScheduleFlushOrCompaction() {
    void DBImpl::BGWorkCompaction(void* arg) {
        void DBImpl::BackgroundCallCompaction(PrepickedCompaction* prepicked_compaction,
                                              Env::Priority bg_thread_pri) {

            Status DBImpl::BackgroundCompaction(bool* made_progress,
                                                JobContext* job_context,
                                                LogBuffer* log_buffer,
                                                PrepickedCompaction* prepicked_compaction,
                                                Env::Priority thread_pri) {
                PickCompaction()
                Compaction* LevelCompactionPicker::PickCompaction(
                    const std::string& cf_name, const MutableCFOptions& mutable_cf_options,
                    const MutableDBOptions& mutable_db_options, VersionStorageInfo* vstorage,
                    LogBuffer* log_buffer) {
                  LevelCompactionBuilder builder(cf_name, vstorage, this, log_buffer,
                                                 mutable_cf_options, ioptions_,
                                                 mutable_db_options);
                  return builder.PickCompaction();
                }   

```
I will put gc function inside MaybeScheduleFlushOrCompaction()
I don't think so. It's too complicated. BGWorkCompaction() will call 
compaction->InstallSuperVersionAndScheduleWork() which is not what I want.
I don't do compaction..
But I need to update version after gc job.
I think I can create a new function to do GC in MaybeScheduleFlushOrCompaction()
current version update is done by calling versionedit and installing new version.
```

    CompactionJob compaction_job(
    compaction_job.Prepare();
    mutex_.Unlock();
    compaction_job.Run().PermitUncheckedError();
    mutex_.Lock();
    status = compaction_job.Install(*c->mutable_cf_options());
    io_s = compaction_job.io_status();
    if (status.ok()) {
      InstallSuperVersionAndScheduleWork(c->column_family_data(),
                                         &job_context->superversion_contexts[0],
                                         *c->mutable_cf_options());
    }
 
```

I can add if else in ColumnFamilyData::PickCompaction() to
pick gc compaction from LevelCompactionPicker
Do I really need to mix compaction and garbage collection job?
We need to delete blob  files after doing gc.
How can we achieve that ?
I think we can add a GCJob which is not Compaction job.
We can do similar version update which simply deletes blob files.
Need to maintain correctness.
```
void DBImpl::InstallSuperVersionAndScheduleWork(
    ColumnFamilyData* cfd, SuperVersionContext* sv_context,
    const MutableCFOptions& mutable_cf_options) {
      // Whenever we install new SuperVersion, we might need to issue new flushes or
      // compactions.
      SchedulePendingCompaction(cfd);
      MaybeScheduleFlushOrCompaction();


```
so SchedulePendingCompaction(cfd) is called after InstallSuperVersionAndScheduleWork()
basically flush triggers level compaction.

Add DBImpl::BGWorkGarbageCollection() interface ;
Call BGWorkGarbageCollection in MaybeScheduleFlushOrCompaction().
NeedsGarbageCollection() will be called in BackgroundGarbageCollection()
and BackgroundCompaction();

how badgerdb do gc for value files
https://github.com/dgraph-io/badger/blob/1c417aa3799cb5010cfc4d520647c769b4b46ba6/value.go#L172
```
fe := func(e Entry) error {
		count+
		if count%100000 == 0 {
			vlog.opt.Debugf("Processing entry %d", count)
		}

		vs, err := vlog.db.get(e.Key)
		if err != nil {
			return err
		}
		if discardEntry(e, vs, vlog.db) {
			return nil
		}

		// Value is still present in value log.
		if len(vs.Value) == 0 {
			return errors.Errorf("Empty value: %+v", vs)
		}
		var vp valuePointer
		vp.Decode(vs.Value)

		// If the entry found from the LSM Tree points to a newer vlog file, don't do anything.
		if vp.Fid > f.fid {
			return nil
		}
		// If the entry found from the LSM Tree points to an offset greater than the one
		// read from vlog, don't do anything.
		if vp.Offset > e.offset {
			return nil
		}
		// If the entry read from LSM Tree and vlog file point to the same vlog file and offset,
		// insert them back into the DB.
		// NOTE: It might be possible that the entry read from the LSM Tree points to
		// an older vlog file. See the comments in the else part.
		if vp.Fid == f.fid && vp.Offset == e.offset {
			moved++
			// This new entry only contains the key, and a pointer to the value.
			ne := new(Entry)
			// Remove only the bitValuePointer and transaction markers. We
			// should keep the other bits.
			ne.meta = e.meta &^ (bitValuePointer | bitTxn | bitFinTxn)
			ne.UserMeta = e.UserMeta
			ne.ExpiresAt = e.ExpiresAt
			ne.Key = append([]byte{}, e.Key...)
			ne.Value = append([]byte{}, e.Value...)
			es := ne.estimateSizeAndSetThreshold(vlog.db.valueThreshold())
			// Consider size of value as well while considering the total size
			// of the batch. There have been reports of high memory usage in
			// rewrite because we don't consider the value size. See #1292.
			es += int64(len(e.Value))

			// Ensure length and size of wb is within transaction limits.
			if int64(len(wb)+1) >= vlog.opt.maxBatchCount ||
				size+es >=vlog.opt.maxBatchSize {
				if err := vlog.db.batchSet(wb); err != nil {
					return err
				}
				size = 0
				wb = wb[:0]
			}
			wb = append(wb, ne)
			size += es
		} else { //nolint:staticcheck

```

Need to know how value file isread during internal iterator seeking.
I think it will first check value type and then open blob file and read 
value from blob file if needed.

```
  if (s.ok()) {
    // Collect iterators for files in L0 - Ln
    if (read_options.read_tier != kMemtableTier) {
      super_version->current->AddIterators(read_options, file_options_,
                                           &merge_iter_builder,
                                           allow_unprepared_value);
    }
          for (int level = 0; level < storage_info_.num_non_empty_levels(); level++) {
            AddIteratorsForLevel(read_options, soptions, merge_iter_builder, level,
                                 allow_unprepared_value);
                table_iter = cfd_->table_cache()->Newiterator(level=0,)
                    FindTable()
                    InternalIterator* result = nullptr;
                  result = table_reader->NewIterator(
                  options, prefix_extractor.get(), arena, skip_filters, caller,
                  file_options.compaction_readahead_size, allow_unprepared_value);

                    return new (mem) BlockBasedTableIterator(
                        this, read_options, rep_->internal_comparator, std::move(index_iter),
                        !skip_filters && !read_options.total_order_seek &&
                            prefix_extractor != nullptr,
                        need_upper_bound_check, prefix_extractor, caller,
                        compaction_readahead_size, allow_unprepared_value);
                            void BlockBasedTableIterator::SeekImpl(const Slice* target,
                                       bool async_prefetch) {
                                if (target) {
                                  block_iter_.Seek(*target);
                                } else {
                                  block_iter_.SeekToFirst();
                                }
                                FindKeyForward();
                              }


 


                              void SeekToRestartPoint(uint32_t index) {
                                raw_key_.Clear();
                                restart_index_ = index;
                                // current_ will be fixed by ParseNextKey();

                                // ParseNextKey() starts at the end of value_, so set value_ accordingly
                                uint32_t offset = GetRestartPoint(index);
                                value_ = Slice(data_ + offset, 0);
                              }


         
 
```
I don't think BlockBasedTableIterator gets value from blob file because 
there is no blob reader in BlockBasedTableIterator() constructor.
Since compaction_iterator use internal iterator to get key and 
use blob fetcher to get blob I think this is similar to what 
we will see in normal internal iterator.


value() method of merging iterator returns current_->value() 
so I think internal iterator won't read blob value.
I should check normal db iterator to get this .


```
  virtual Iterator* NewIterator(const ReadOptions& options,
                                ColumnFamilyHandle* column_family) override;


        //db_iter.cc
        Iterator* NewDBIterator(Env* env, const ReadOptions& read_options,
                   const ImmutableOptions& ioptions,
                                const MutableCFOptions& mutable_cf_options,
                                const Comparator* user_key_comparator,
                                InternalIterator* internal_iter, const Version* version,
                                const SequenceNumber& sequence,
                                uint64_t max_sequential_skip_in_iterations,
                                ReadCallback* read_callback, DBImpl* db_impl,
                                ColumnFamilyData* cfd, bool expose_blob_index) {
          DBIter* db_iter =
              new DBIter(env, read_options, ioptions, mutable_cf_options,
                         user_key_comparator, internal_iter, version, sequence, false,
                         max_sequential_skip_in_iterations, read_callback, db_impl, cfd,
                         expose_blob_index);
          return db_iter;
            }
                bool DBIter::FindNextUserEntryInternal(bool skipping_saved_key,
                                       const Slice* prefix) {
                      case kTypeValue:
                      case kTypeBlobIndex:
                      case kTypeWideColumnEntity:
                        if (timestamp_lb_) {
                          saved_key_.SetInternalKey(ikey_);
                        } else {
                          saved_key_.SetUserKey(
                              ikey_.user_key, !pin_thru_lifetime_ ||
                                                  !iter_.iter()->IsKeyPinned() /* copy */);
                        }

                        if (ikey_.type == kTypeBlobIndex) {
                          if (!SetBlobValueIfNeeded(ikey_.user_key, iter_.value())) {
                            return false;
                          }
             

                            bool DBIter::SetBlobValueIfNeeded(const Slice& user_key,
                                                              const Slice& blob_index) {
                              assert(!is_blob_);
                              assert(blob_value_.empty());

                              const Status s = version_->GetBlob(read_options, user_key, blob_index,
     
```
Found the place where blob value is read.
So now should I used db_iter or internal iter?
I can just copy bool DBIter::SetBlobValueIfNeeded(const Slice& user_key,
to my gc job and then do blob read 
db_iter.cc
```
bool DBIter::SetBlobValueIfNeeded(const Slice& user_key,
                    const Slice& blob_index) {
  assert(!is_blob_);

  
  const Status s = version_->GetBlob(read_options, user_key, blob_index,
                                     prefetch_buffer, &blob_value_, bytes_read);

 
```
Actually I don't need to cal version_->GetBlob()
I just need to get value of internal iterator and put keys with sequence number back 
into rocksdb.

Does offset in BlobLogSequentialReader the same as that in what
we get in compaction iterator?
```
Status BlobLogSequentialReader::ReadRecord(BlobLogRecord* record,
                                           ReadLevel level,
                                           uint64_t* blob_offset) {
  assert(record);
 
```

```

bool CompactionIterator::ExtractLargeValueIfNeededImpl() {
    s = blob_file_builder_->Add(user_key(), value_, &blob_index_);

        Status BlobFileBuilder::WriteBlobToFile(const Slice& key, const Slice& blob,
                                                uint64_t* blob_file_number,
                                                uint64_t* blob_offset) {
         
            Status BlobLogWriter::AddRecord(const Slice& key, const Slice& val,
              Status s = EmitPhysicalRecord(buf, key, val, key_offset, blob_offset);
                  *blob_offset = *key_offset + key.size();

      BlobIndex::EncodeBlob(blob_index, blob_file_number, blob_offset, blob.size(),
                            blob_compression_type_);


```

Need to call DecodeBlob() to get blob file number and blob offset.
```
class BlobIndex {
  Status DecodeFrom(Slice slice) {
```
So I think it's the same between what we get from blob reader
and what we get from internal iterator.
So now we can do comparation between file number and file offset
to determine if blob value in blob file is valid and should be put 
back into rocksdb .

May need to write some basic test to test gc job running.
first of all, let's trigger gc jobs .


Need to update bytes_read_blob in compaction_stats
Don't know the difference between UpdateCompactionStats() and UpdateCompactionJobStats()
```

void CompactionJob::UpdateCompactionStats() {
      UpdateCompactionInputStatsHelper(
          &compaction_stats_.stats.num_input_files_in_non_output_levels,
          &compaction_stats_.stats.bytes_read_non_output_levels, input_level);
      assert(compaction_job_stats_);
      compaction_stats_.stats.bytes_read_blob =
          compaction_job_stats_->total_blob_bytes_read;



void CompactionJob::UpdateCompactionJobStats(
    const InternalStats::CompactionStats& stats) const {
  compaction_job_stats_->elapsed_micros = stats.micros;

  // input information
  compaction_job_stats_->total_input_bytes =
      stats.bytes_read_non_output_levels + stats.bytes_read_output_level;
  compaction_job_stats_->num_input_records = stats.num_input_records;
  compaction_job_stats_->num_input_files =
      stats.num_input_files_in_non_output_levels +
      stats.num_input_files_in_output_level;
  compaction_job_stats_->num_input_files_at_output_level =
      stats.num_input_files_in_output_level;


```


```
The `CompactionJob::RecordCompactionIOStats()` function in RocksDB is used to record the I/O statistics related to a compaction job. Here's a breakdown of the code:

void CompactionJob::RecordCompactionIOStats() {
  RecordTick(stats_, COMPACT_READ_BYTES, IOSTATS(bytes_read));
  RecordTick(stats_, COMPACT_WRITE_BYTES, IOSTATS(bytes_written));
}

- `RecordTick`: This is a function used to update a statistics counter in RocksDB. It takes three arguments: a pointer to a `Statistics` object, a `Tickers` enum value representing the specific counter to update, and the count to add to the counter.

- `stats_`: This is a pointer to a `Statistics` object where various runtime statistics are recorded.

- `COMPACT_READ_BYTES` and `COMPACT_WRITE_BYTES`: These are enum values from the `Tickers` enum. They represent counters for the number of bytes read and written during compaction, respectively.

- `IOSTATS(bytes_read)` and `IOSTATS(bytes_written)`: These are macros that fetch the number of bytes read from and written to storage during the compaction job. The `IOSTATS` macro is used to access thread-local I/O statistics.

So, in summary, this function updates the compaction read and write byte counters in the `Statistics` object with the number of bytes read and written during the compaction job¹². This can be useful for monitoring the I/O performance of RocksDB compactions. Please note that the actual usage may vary based on the context and the specific requirements of your application. For more detailed usage, you may want to look at the RocksDB source code¹.

Source: Conversation with Bing, 1/8/2024
(1) Compaction Stats and DB Status · facebook/rocksdb Wiki · GitHub. https://github.com/facebook/rocksdb/wiki/Compaction-Stats-and-DB-Status.
(2) Compaction · facebook/rocksdb Wiki · GitHub. https://github.com/facebook/rocksdb/wiki/Compaction.
(3) Reduce Write Amplification by Aligning Compaction Output File ... - RocksDB. https://rocksdb.org/blog/2022/10/31/align-compaction-output-file.html.
(4) Manual Compaction · facebook/rocksdb Wiki · GitHub. https://github.com/facebook/rocksdb/wiki/Manual-Compaction.
(5) undefined. https://smalldatum.blogspot.com/2018/08/name-that-compaction-algorithm.html.
```


Read this file to learn about how blob compaction is tested.
```
        db/blob/db_blob_compaction_test.cc
```

sst files are marked being compacted during construction of compaction.
So during ComputeCompactionScore() those marked files will not be 
considered .
```
        _compaction_reason == CompactionReason::kExternalSstIngestion ||
                  _compaction_reason == CompactionReason::kRefitLevel
              ? Compaction::kInvalidLevel
              : EvaluatePenultimateLevel(vstorage, immutable_options_,
                                         start_level_, output_level_)) {
  MarkFilesBeingCompacted(true);
  i
```

There is only one 'nothing to do ' log in LOG file which is not normal.
And there is exit error during db destructino. Need to figure out 
why and fix it.

I need to write a method to collect training data during run time.
Fuck, there are stll two big work items to do.
I will finish current gc process asap and then start data collection 
module

AddToGarbageCollectionQueue ref cfd. 
Where does unref happen? 
unref happens in BackgroundGarbageCollection
But refs_ is 2 at the end of execution.
So there might be once call to ref but there is no call to unref.
Need to find out where ref is called.
```
void DBImpl::AddToGarbageCollectionQueue(ColumnFamilyData* cfd) {
  assert(!cfd->queued_for_garbage_collection());
  cfd->Ref();
  gc_queue_.push_back(cfd);
  cfd->set_queued_for_garbage_collection(true);
}

    if (cfd->UnrefAndTryDelete()) {
      // This was the last reference of the column family, so no need to
      // compact.
      return Status::OK();
    }



gdb) p cfd->refs_
$1 = std::atomic<int> = { 2 }
```

Fix two bugs
1. update if(gc_queue_) from if(compaction_queue_) in gc job. 
2. add gc_queue clean up in closehlper()
```
  while(!gc_queue_.empty()) {
    auto cfd = PopFirstFromGarbageCollectionQueue();
    cfd->UnrefAndTryDelete();
  }


```

Fixed the ref bug after fix above
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50    p99      p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    versionjob_id   githash
0               153MB   6GB     7.2     1.7     3.9     503     502     0       6       1778525656.0   1895     0.0     0       1.8     0.1     5.7     replay.t1.s1    2024-01-9T22:17:11      8.0.0          ef6523a70e       0.9
```

[Todo]
Add PickGarbageCollection to LevelCompactionPicker
Marked blob file being gced in construction of GarbageCollection
compactions_in_progress_ check.
how is compactions_in_progress_ used to prevent duplicated compaction?.
```
bool CompactionPicker::RangeOverlapWithCompaction(
    const Slice& smallest_user_key, const Slice& largest_user_key,
    int level) const {
  const Comparator* ucmp = icmp_->user_comparator();
  for (Compaction* c : compactions_in_progress_) {
    if (c->output_level() == level &&

Compaction* LevelCompactionBuilder::GetCompaction() {
  auto c = new Compaction(
      vstorage_, ioptions_, mutable_cf_options_, mutable_db_options_,
      std::move(compaction_inputs_), output_level_,
      MaxFileSizeForLevel(mutable_cf_options_, output_level_,
                          ioptions_.compaction_style, vstorage_->base_level(),
                          ioptions_.level_compaction_dynamic_level_bytes),
      mutable_cf_options_.max_compaction_bytes,
      GetPathId(ioptions_, mutable_cf_options_, output_level_),
      GetCompressionType(vstorage_, mutable_cf_options_, output_level_,
                         vstorage_->base_level()),
      GetCompressionOptions(mutable_cf_options_, vstorage_, output_level_),
      Temperature::kUnknown,
      /* max_subcompactions */ 0, std::move(grandparents_), is_manual_,
      /* trim_ts */ "", start_level_score_, false /* deletion_compaction */,
      /* l0_files_might_overlap */ start_level_ == 0 && !is_l0_trivial_move_,
      compaction_reason_);

  // If it's level 0 compaction, make sure we don't execute any other level 0
  // compactions in parallel
  compaction_picker_->RegisterCompaction(c);
  }



    void CompactionPicker::RegisterCompaction(Compaction* c) {
      if (c == nullptr) {
        return;
      }
      assert(ioptions_.compaction_style != kCompactionStyleLevel ||
             c->output_level() == 0 ||
             !FilesRangeOverlapWithCompaction(*c->inputs(), c->output_level(),
                                              c->GetPenultimateLevel()));
      // CompactionReason::kExternalSstIngestion's start level is just a placeholder
      // number without actual meaning as file ingestion technically does not have
      // an input level like other compactions
      if ((c->start_level() == 0 &&
           c->compaction_reason() != CompactionReason::kExternalSstIngestion) ||
          ioptions_.compaction_style == kCompactionStyleUniversal) {
        level0_compactions_in_progress_.insert(c);
      }
      compactions_in_progress_.insert(c);
      TEST_SYNC_POINT_CALLBACK("CompactionPicker::RegisterCompaction:Registered",
                               c);
    }


// Delete this compaction from the list of running compactions.
void CompactionPicker::ReleaseCompactionFiles(Compaction* c, Status status) {
  UnregisterCompaction(c);
  if (!status.ok()) {
    c->ResetNextCompactionIndex();
  }
}
    void CompactionPicker::UnregisterCompaction(Compaction* c) {
      if (c == nullptr) {
        return;
      }
      if (c->start_level() == 0 ||
          ioptions_.compaction_style == kCompactionStyleUniversal) {
        level0_compactions_in_progress_.erase(c);
      }
      compactions_in_progress_.erase(c);
    }



void LevelCompactionBuilder::SetupInitialFiles() {

bool LevelCompactionBuilder::SetupOtherInputsIfNeeded() {
    bool CompactionPicker::FilesRangeOverlapWithCompaction(
      return RangeOverlapWithCompaction(smallest.user_key(), largest.user_key(),
                                        level);



bool LevelCompactionBuilder::PickFileToCompact() {

        compaction_picker_->FilesRangeOverlapWithCompaction(
            {start_level_inputs_}, output_level_,
            Compaction::EvaluatePenultimateLevel(
                vstorage_, ioptions_, start_level_, output_level_))) {

              return RangeOverlapWithCompaction(smallest.user_key(), largest.user_key(),
                                                level);

                  for (Compaction* c : compactions_in_progress_) {
                    if (c->output_level() == level &&
                        ucmp->CompareWithoutTimestamp(smallest_user_key,
                                                      c->GetLargestUserKey()) <= 0 &&
                        ucmp->CompareWithoutTimestamp(largest_user_key,
                                                      c->GetSmallestUserKey()) >= 0) {
                      // Overlap
                      return true;
                    }
 

 
```

Can call ParsedInternalKey to get key type.
```
bool DBIter::ParseKey(ParsedInternalKey* ikey) {
  Status s = ParseInternalKey(iter_.key(), ikey, false /* log_err_key */);
  if (!s.ok()) {
    status_ = Status::Corruption("In DBIter: ", s.getState());
    valid_ = false;
    ROCKS_LOG_ERROR(logger_, "In DBIter: %s", status_.getState());
    return false;
  } else {
    return true;
  }
}


```


So basically `Compaction* LevelCompactionBuilder::PickCompaction() {`
does overlap check for current copmaction and return nullptr if there is 
overlap between current to-be-picked compaction and picked compaction.
I will do this similar overlap check as well.
[Status: Done]

```

Compaction* LevelCompactionBuilder::PickCompaction() {
  Compaction* c = GetCompaction();
        Compaction* LevelCompactionBuilder::GetCompaction() {
          compaction_picker_->RegisterCompaction(c);
          vstorage_->ComputeCompactionScore(ioptions_, mutable_cf_options_);
```
Need to do unscheduled_garbage_collectinos_++ in scheduled_garbage_collection function
garbage collection picker process
```

gc.reset(cfd->PickGarbageCollection(*mutable_cf_options, mutable_db_options_,
    GarbageCollection* ColumnFamilyData::PickGarbageCollection(const MutableCFOptions& mutable_options,
                                 const MutableDBOptions& mutable_db_options,
                                 LogBuffer* log_buffer) {

      auto* result = garbage_collection_picker_->PickGarbageCollection(
          GetName(), mutable_options, mutable_db_options, current_->storage_info(),

          vstorage->ComputeBlobsMarkedForForcedGC( mutable_cf_options.blob_garbage_collection_age_cutoff) ;
 
```

compute compaction file process
```

compaction_picker_level.cc

Compaction* LevelCompactionBuilder::PickCompaction() {
    Compaction* LevelCompactionBuilder::GetCompaction() {
        void VersionStorageInfo::ComputeCompactionScore(
            const ImmutableOptions& immutable_options,
            const MutableCFOptions& mutable_cf_options) {

           ComputeFilesMarkedForCompaction();

Status VersionSet::ProcessManifestWrites(
    std::deque<ManifestWriter>& writers, InstrumentedMutex* mu,
    FSDirectory* dir_contains_current_file, bool new_descriptor_log,
    const ColumnFamilyOptions* new_cf_options) {
 
    void VersionSet::AppendVersion(ColumnFamilyData* column_family_data,
                                   Version* v) {
      // compute new compaction score
      v->storage_info()->ComputeCompactionScore(
          *column_family_data->ioptions(),
          *column_family_data->GetLatestMutableCFOptions());


Status DBImpl::CompactFilesImpl(
    const CompactionOptions& compact_options, ColumnFamilyData* cfd,
    Version* version, const std::vector<std::string>& input_file_names,
    std::vector<std::string>* const output_file_names, const int output_level,
    int output_path_id, JobContext* job_context, LogBuffer* log_buffer,
    CompactionJobInfo* compaction_job_info) {
 
  CompactionJob compaction_job(
  version->storage_info()->ComputeCompactionScore(*cfd->ioptions(),
                                                  *c->mutable_cf_options());


```
So I think I need to do ComputeBlobsMarkedForForcedGC() in VersionSet::AppendVersion()
to trigger first ComputeBlobsMarkedForForcedGC();
gdb breaks at PickGarbageCollectionFromQueue() after I add ComputeBlobsMarkedForForcedGC in AppendVersion()

Finally see gc blob file list in log in gc job run function.
But there is last_ref issue when db closing. 
Need to deal with that.
cfd->refs_ is 15 which is not normal.
```
(gdb) p cfd->refs_
$2 = std::atomic<int> = { 15 }

```

Add bg_gc_scheduled_ in WaitForBackgroundWork() to wait for gc job 
in CloseHelper() to finish to fix ref problem.
```
void DBImpl::WaitForBackgroundWork() {
  // Wait for background work to finish
  while (bg_bottom_compaction_scheduled_ || bg_compaction_scheduled_ ||
         bg_flush_scheduled_ || bg_gc_scheduled_) {
    bg_cv_.Wait();
  }
}
```
bg_gc_scheduled_ to indicate gc work scheduled
num_running_gcs_ to indicate how many gcs are running
unscheduled_garbage_collections_ indicate how many unscheduled_garbage_collections_
Fixed it after I did this.
[Todo]
Put keys back to db during gc job.
I don't know how to check validity of keys yet.
[Status: Done]

[Todo]
Delete blob files after keys are put back to rocksdb.
Need to check bytes_read .
No bytes_written is needed because this is done by rocksdb.
Where does file deletion happen?
```
Status GarbageCollectionJob::InstallGarbageCollectionResults(const MutableCFOptions& mutable_cf_options) {
```

```
Install()

  db_mutex_->AssertHeld();

  cfd->internal_stats()->AddCompactionStats(output_level, thread_pri_,
  status = InstallCompactionResults(mutable_cf_options);
      VersionEdit* const edit = compaction->edit();
      assert(edit);
      // Add compaction inputs
      compaction->AddInputDeletions(edit);

        for (const auto& blob : sub_compact.Current().GetBlobFileAdditions()) {
          edit->AddBlobFile(blob);
        }



      return versions_->LogAndApply(compaction->column_family_data(),
  CleanupCompaction();
InstallSuperVersionAndScheduleWork(c->column_family_data(),
                                 &job_context->superversion_contexts[0],
                                 *c->mutable_cf_options());
 

c->ReleaseCompactionFiles(status);
```

```
    ReleaseFileNumberFromPendingOutputs(pending_outputs_inserted_elem);

    FindObsoleteFiles(&job_context, !s.ok() && !s.IsShutdownInProgress() &&
        versions_->GetObsoleteFiles(
              &job_context->sst_delete_files, &job_context->blob_delete_files,
              &job_context->manifest_delete_files, job_context->min_pending_output);

              for (auto& f : obsolete_files_) {
                if (f.metadata->fd.GetNumber() < min_pending_output) {
                  files->emplace_back(std::move(f));
                } else {
                  pending_files.emplace_back(std::move(f));
                }
              }

              obsolete_files_.swap(pending_files);
        for (const auto& blob_file : job_context->blob_delete_files) {
            MarkAsGrabbedForPurge(blob_file.GetBlobFileNumber());
          }


    PurgeObsoleteFiles(job_context);
```
I need to do file deltion that is similar to version update in comaction.
How is obsolete_files_ are built and when are file deleted?
I don't think I need to care about other vars right.
What is job_context used for?

obsolete_blob_files_ 
```
    auto deleter = [vs, ioptions](SharedBlobFileMetaData* shared_meta) {
      if (vs) {
        assert(ioptions);
        assert(!ioptions->cf_paths.empty());
        assert(shared_meta);

        vs->AddObsoleteBlobFileWithLifetime(shared_meta->GetBlobFileNumber(),
                                            ioptions->cf_paths.front().path,
                                            shared_meta->GetLifetimeLabel());
        // vs->AddObsoleteBlobFile(shared_meta->GetBlobFileNumber(),
        //                         ioptions->cf_paths.front().path);
      }

      delete shared_meta;
    };


      void AddObsoleteBlobFileWithLifetime(uint64_t blob_file_number, std::string path,
                                          uint64_t lifetime) {
        assert(table_cache_);

        table_cache_->Erase(GetSliceForKey(&blob_file_number));

        obsolete_blob_files_.emplace_back(blob_file_number, std::move(path));
        }
 
```
blob files are discard in version install and apply.
So I will need to create a version edit to delete blob file that are used
in gc.
This should work.
Will verify this in Log.

```


Status VersionSet::ProcessManifestWrites(
    Status VersionSet::LogAndApplyHelper(ColumnFamilyData* cfd,
        Status Apply(const VersionEdit* edit) {

          Status ApplyBlobFileAddition(const BlobFileAddition& blob_file_addition) {

            for (const auto& deleted_file : edit->GetDeletedFiles()) {


Status s = builder->SaveTo(versions[i]->storage_info());
    Status VersionBuilder::SaveTo(VersionStorageInfo* vstorage) const {
        return rep_->SaveTo(vstorage);

            SaveBlobFilesTo(vstorage);

               const uint64_t oldest_blob_file_with_linked_ssts =    GetMinOldestBlobFileNumber();
              MergeBlobFileMetasWithLifetime(oldest_blob_file_with_linked_ssts, process_base,
```

```

for (const auto& deleted_file : edit->GetDeletedFiles()) {
  Status ApplyFileDeletion(int level, uint64_t file_number) {
    del_files.emplace(file_number);

    table_file_levels_[file_number] =
        VersionStorageInfo::FileLocation::Invalid().GetLevel();

 
```
So SaveBlobFilesTo() will set up a minimum blob file number that has
sst link. older blob file will not be added to new blob_files_ in new version.
I will just add deleted file in version edit and not put blob files in 
new VersionStorageInfo in saveto if blob files are in deleted files in 
verionedit..

```
Status CompactionJob::InstallCompactionResults(
  VersionEdit* const edit = compaction->edit();

  compaction->AddInputDeletions(edit);

  for (const auto& sub_compact : compact_->sub_compact_states) {
    sub_compact.AddOutputsEdit(edit);
          void AddOutputsEdit(VersionEdit* out_edit) const {
            for (const auto& file : penultimate_level_outputs_.outputs_) {
              out_edit->AddFile(compaction->GetPenultimateLevel(), file.meta);
            }
            for (const auto& file : compaction_outputs_.outputs_) {
              out_edit->AddFile(compaction->output_level(), file.meta);
            }
          }



    for (const auto& blob : sub_compact.Current().GetBlobFileAdditions()) {
      edit->AddBlobFile(blob);
    }


```
edit is updated in InstallCompactionResults()
So no versionedit update in ProcessKeyValueCompaction.
So how blob file update and sstfile addition is done in ProcessKeyValueCompaction
and in 

Need to unlink blob files if blob file are deleted during gc job.
Don't think I need to do this.
Because sst deletion involes blob file unlinking sst but 
there is no need to unlink blob files in sst.

Will need to discard keys in lsm-tree during compaction. Can check if blob 
file number exists in all blob files.

Got assertion failed during flush.
Because I write new keys back into rocksdb 
so compaction iterator call get  features function to 
give classification for newly written keys during flush.
How should I deal with that?
I can just put keys at short lifetime classification for now.
Later I can specify a special type of key with classification 
number in it.

blob file are not deleted during gc.
Met seg fault. I don't know why. Let's figure that out.
I think this is because there is recursion when I do
gc for blob files that have values for same key.
How can I fix this problem?
Maybe I need to use some fixed sequence number?
Let's see how badgerdb works with this problem and read the article..

```
gcc -g -o memleak memleak.c
valgrind --leak-check=full ./memleak
```
I see blob file:11  deletion in LOG . Why is that ?
```

2024/01/14-16:46:54.872901 1294888 [db/compaction/compaction_job.cc:1666] [default] [JOB 16] Generated table #70: 96642 keys, 3408294 bytes, temperature: kUnknown

2024/01/14-16:53:24.410992 1294895 EVENT_LOG_v1 {"time_micros": 1705222404410974, "job": 74, "event": "gc_started", "gc_files": [11, 15, 19, 23, 31, 35, 39, 43, 54, 58, 62, 66, 80, 84, 88, 91, 101]}

024/01/14-16:53:39.297769 1294895 (Original Log Time 2024/01/14-16:53:39.285025) EVENT_LOG_v1 {"time_micros": 1705222419284991, "job": 74, "valid_key_num": 16371, "invalid_key_num": 297926, "valid_value_size": 13054183, "invalid_value_size": 240403770, "invalid_key_ratio": 0.947912}
2024/01/14-16:53:39.298130 1294895 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000070.sst immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2024/01/14-16:53:39.298140 1294895 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 74] Delete /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000070.sst type=2 #70 -- OK
2024/01/14-16:53:39.298150 1294895 EVENT_LOG_v1 {"time_micros": 1705222419298147, "job": 74, "event": "table_file_deletion", "file_number": 70}
2024/01/14-16:53:39.298383 1294895 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000011.blob immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2024/01/14-16:53:39.298388 1294895 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 74] Delete /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000011.blob type=10 #11 -- OK
2024/01/14-16:53:39.298394 1294895 EVENT_LOG_v1 {"time_micros": 1705222419298393, "job": 74, "event": "blob_file_deletion", "file_number": 11}
2024/01/14-16:53:40.197131 1294917 [db/db_impl/db_impl.cc:1123] ------- DUMPING
```
lots of file are involved in gc but only blob:11 is deleted.  Why is that ?


Only one gc job is allowed at a time?
badger says that only one gc job is allowed .
get searches each level to get a key with max version.
How does put works or how does gc works in terms of version changing 
for each key?
```

https://github.com/dgraph-io/badger/blob/1c417aa3799cb5010cfc4d520647c769b4b46ba6/db.go#L1239
func (db *DB) RunValueLogGC(discardRatio float64) error {
    // batchSet applies a list of badger.Entry. If a request level error occurs it
    // will be returned.
    //
    //	Check(kv.BatchSet(entries))
    func (db *DB) batchSet(entries []*Entry) error {
        req, err := db.sendToWriteCh(entries)
        if err != nil {
            return err
        }

        return req.Wait()
    }


// get returns value for a given key or the key after that. If not found, return nil.
func (s *levelHandler) get(key []byte) (y.ValueStruct, error) {
	tables, decr := s.getTableForKey(key)
	keyNoTs := y.ParseKey(key)

	hash := y.Hash(keyNoTs)
	var maxVs y.ValueStruct
	for _, th := range tables {
		if th.DoesNotHave(hash) {
			y.NumLSMBloomHitsAdd(s.db.opt.MetricsEnabled, s.strLevel, 1)
			continue
		}

		it := th.NewIterator(0)
		defer it.Close()

		y.NumLSMGetsAdd(s.db.opt.MetricsEnabled, s.strLevel, 1)
		it.Seek(key)
		if !it.Valid() {
			continue
		}
		if y.SameKey(key, it.Key()) {
			if version := y.ParseTs(it.Key()); maxVs.Version < version {
				maxVs = it.ValueCopy()
				maxVs.Version = version
			}
		}
	}
	return maxVs, decr()
}



	seek := y.KeyWithTs(key, txn.readTs)
	vs, err := txn.db.get(seek)
```
Looks like badger uses timestamp as sequence number and seek with readts.
I don't see how ts is added to key during put.
Fixed the seg fault after I add deleted blob file check at the end of 
`  void MergeBlobFileMetasWithLifetime(uint64_t first_blob_file, ProcessBase process_base,`
function.
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_op p50     p99p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test    date    version job_id  githash
0               43MB    1GB     1.3     1.7     2.8     83      83      0       1       335189898.0       460      0.0     0       0.4     0.0     4.4     replay.t1.s1    2024-01-15T20:17:27     8.0.0           41ac5b042d 0.9
cp: '/mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/LOG' and '/mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/LOG' are the same file
```
Checked LOG and found there is verion builder error mentioning blob:15 not found.
Found ApplyBlobFileGarbage() function containing this information.
Now I'am trying to fix this error by removing this function.
```
  void AddBlobFileGarbage(uint64_t blob_file_number,
                          uint64_t garbage_blob_count,
                          uint64_t garbage_blob_bytes) {
    blob_file_garbages_.emplace_back(blob_file_number, garbage_blob_count,
                                     garbage_blob_bytes);
 
```
Remove ApplyBlobFileGarbage() in version_builder::Apply()
Still get unaligned fastbin chunk detected error after removing ApplyBlobFileGarbage()
I think this is because of adding while iterating the iterator .
Why is that ?
Is this because memtable not released after gc job ??
Did not get mem error after adding memory usage function which is not normal. 
Check LOG. only 7000k writes not 1M I don't know why. 
```
2024/01/16-11:40:45.523965 1219657 EVENT_LOG_v1 {"time_micros": 1705376445523963, "job": 166, "event": "blob_file_deletion", "file_number": 42}
2024/01/16-11:40:45.529170 1219657 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000038.blob immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2024/01/16-11:40:45.529180 1219657 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 166] Delete /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000038.blob type=10 #38 -- OK
2024/01/16-11:40:45.529187 1219657 EVENT_LOG_v1 {"time_micros": 1705376445529185, "job": 166, "event": "blob_file_deletion", "file_number": 38}
2024/01/16-11:40:45.534504 1219657 [file/delete_scheduler.cc:73] Deleted file /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000034.blob immediately, rate_bytes_per_sec 0, total_trash_size 0 max_trash_db_ratio 0.250000
2024/01/16-11:40:45.534513 1219657 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 166] Delete /mnt/nvme1n1/mlsm/test_blob_with_model/with_model_gc_1.0_0.8/000034.blob type=10 #34 -- OK
2024/01/16-11:40:45.534532 1219657 EVENT_LOG_v1 {"time_micros": 1705376445534519, "job": 166, "event": "blob_file_deletion", "file_number": 34}
```

```
"lifetime_blob_0": [11, 15, 19, 23, 31, 35, 39, 43, 54, 58, 62], 
"lifetime_blob_1": [10, 14, 18, 22, 30, 34, 38, 42, 53, 57, 61]


2024/01/16-11:40:18.856002 1219657 EVENT_LOG_v1 {"time_micros": 1705376418855996, "job": 164, "event": "gc_started", "gc_files": [241, 10]}

2024/01/16-11:40:32.202725 1219657 EVENT_LOG_v1 {"time_micros": 1705376432202720, "job": 166, "event": "gc_started", "gc_files": [250, 14]}
```

Switch to write batch write instead of calling put() for each key.
without_model LOG
```
** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      2/0    6.58 MB   0.5      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     81.7     79.92             79.54        82    0.975       0      0       0.0       6.1
  L1      9/1   26.43 MB   0.9     14.4     0.3      0.4       0.6      0.2       0.0   1.0     31.0     30.9    476.06            474.84        20   23.803     18M  1514K      13.8      13.8
  L2     45/3   125.06 MB   0.6      7.4     0.1      0.2       0.3      0.1       0.0   1.0     30.7     30.5    248.21            247.59        39    6.364     10M  1592K       7.1       7.1
 Sum     56/4   158.07 MB   0.0     21.8     0.4      0.6       1.1      0.5       0.0   4.4     27.8     35.8    804.19            801.97       141    5.704     29M  3107K      20.8      27.0
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s)Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0     21.8     0.4      0.6       0.9      0.3       0.0   0.0     30.9     30.7    724.27            722.43        59   12.276     29M  3107K      20.8      20.8
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     81.7     79.92             79.54        82    0.975       0      0       0.0       6.1

Blob file count: 53, total size: 4.3 GB, garbage size: 0.6 GB, space amp: 1.2


```
with_model LOG
```
** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      2/0    7.20 MB   0.5      0.0     0.0      0.0       0.2      0.2       0.0   1.0      0.0     51.2    104.94            104.56        66    1.590       0      0       0.0       5.0
  L1      6/0   24.88 MB   1.0      0.5     0.2      0.3       0.5      0.2       0.0   2.2      3.9      3.6    130.77            130.61        16    8.173     14M  1010K       0.0       0.0
  L2     38/0   91.16 MB   0.5      0.2     0.1      0.1       0.2      0.1       0.0   1.7      4.3      3.3     59.09             58.98        34    1.738   7418K  1741K       0.0       0.0
 Sum     46/0   123.24 MB   0.0      0.7     0.3      0.4       0.9      0.4       0.0   1.1      2.6     20.5    294.80            294.16       116    2.541     22M  2752K       0.0       5.0
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      0.7     0.3      0.4       0.7      0.2       0.0   0.0      4.0      3.5    189.86            189.59        50    3.797     22M  2752K       0.0       0.0
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.2      0.2       0.0   0.0      0.0     51.2    104.94            104.56        66    1.590       0      0       0.0       5.0

Blob file count: 50, total size: 2.9 GB, garbage size: 0.0 GB, space amp: 1.0


2024/01/16-18:17:50.769441 12272 (Original Log Time 2024/01/16-18:17:50.769241) [db/db_impl/db_impl_compaction_flush.cc:298] [default] Level summary: files[3 6 38 0 0 0 0 0] max score 1.00
Completed replay (ID: ) in 2086 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               127MB   2GB     6.0     1.9     5.3     296     295     0       5       1033092328.0                                            1148    0.0     0       1.3     0.1     5.9    replay.t1.s1     2024-01-16T18:00:37     8.0.0           41ac5b042d      0.8

```
Remove  AddWithFeatures from memtable.cc and restart replay.
No seg fault this time but stil only 7611k writes.
Let's  remove db->put() from gc job and see if we can get 1M writes.

Only 2017k writes at the end of replay Let's see what's wrong.
Is this because I set num_writes to 2M in call_run_blob.sh?
num_keys=50000000
Need to check how replay works
I think this is because get op get status not ok and replay failed.
So this is why out current replay failing to replay all writes.


Found invalid blob file number during run time. So I search
invalid blob file number in all files and found GetBlob() function
contains this log msg. I add assert(false); to this function and 
see what's wrong.

blob:88 not exist in GetBlob()
I found 88 blob file why is that ?
Is this because blob file not added in GetMinOldestBlobFileNumber?

```
000088.blob
2024/01/17-15:21:26.715345 489412 (Original Log Time 2024/01/17-15:21:26.714864) EVENT_LOG_v1 {"time_micros": 1705476086714796, "job": 159, "event": "compaction_finished", "compaction_time_micros": 17296315, "compaction_time_cpu_micros": 17286682, "output_level": 1, "num_output_files": 13, "total_output_size": 39680315, "num_input_records": 1136283, "num_output_records": 1096304, "num_subcompactions": 1, "output_compression": "NoCompression", "num_single_delete_mismatches": 0, "num_single_delete_fallthrough": 0, "lsm_state": [1, 13, 36, 0, 0, 0, 0, 0], "blob_file_head": 282, "blob_file_tail": 449, "blob_files": [282, 285, 298, 301, 304, 305, 315, 327, 337, 340, 353, 366, 369, 372, 373, 381, 392, 393, 403, 404, 407, 415, 424, 435, 440, 449], "lifetime_blob_0": [282, 298, 301, 304, 315, 327, 337, 340, 353, 366, 369, 372, 381, 392, 403, 407, 415, 424, 435, 440, 449], "lifetime_blob_1": [285, 305, 373, 393, 404]}
```


```
ProcessKeyValueCompaction()
  if (sub_compact->compaction->DoesInputReferenceBlobFiles()) {
    BlobGarbageMeter* meter = sub_compact->Current().CreateBlobGarbageMeter();
    blob_counter = std::make_unique<BlobCountingIterator>(input, meter);
    input = blob_counter.get();
  }
  void Next() override {
    assert(Valid());

    iter_->Next();
    UpdateAndCountBlobIfNeeded();
  }


```
Does new sst file link original blob files?
I don't think I make this step.
```
Status BlobGarbageMeter::ProcessOutFlow(const Slice& key, const Slice& value) {
  // Note: in order to measure the amount of additional garbage, we only need to
  // track the outflow for preexisting files, i.e. those that also had inflow.
  // (Newly written files would only have outflow.)
  auto it = flows_.find(blob_file_number);
  if (it == flows_.end()) {
    return Status::OK();
  }

  it->second.AddOutFlow(bytes);


    uint64_t GetGarbageCount() const {
      assert(IsValid());
      assert(HasGarbage());
      return in_flow_.GetCount() - out_flow_.GetCount();
    }




          for (const auto& pair : flows) {
        const uint64_t blob_file_number = pair.first;
        const BlobGarbageMeter::BlobInOutFlow& flow = pair.second;

        assert(flow.IsValid());
        if (flow.HasGarbage()) {
          blob_total_garbage[blob_file_number].Add(flow.GetGarbageCount(),
                                                   flow.GetGarbageBytes());
    }
      }
    }
  }

  for (const auto& pair : blob_total_garbage) {
    const uint64_t blob_file_number = pair.first;
    const BlobGarbageMeter::BlobStats& stats = pair.second;

    edit->AddBlobFileGarbage(blob_file_number, stats.GetCount(),
                             stats.GetBytes());
  }
```
So ProcessOutFlow means new blobs out?
May need to link sst for each lifetime classes.
I am trying to get log from version storage and then log min oldest blob file number
I think this is the problem, the min oldest log number might be too high.  
Set min_oldest_blob_file_numer to zero instead of get blob files with linked sst.
Now log looks normal . There are log entries showing blob files being deleted.
Fixed seg fault bug in Get after code update above.

[Status: Done ]
```
** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      5/4   19.54 MB   0.2      0.0     0.0      0.0       0.5      0.5       0.0   1.0      0.0     61.1    222.14            221.52       153    1.452       0      0       0.0      12.7
  L1      8/8   24.60 MB   0.0      1.3     0.5      0.8       1.2      0.5       0.0   2.3      3.8      3.6    350.55            350.23        37    9.474     38M  1759K       0.0       0.0
  L2     50/0   132.73 MB   0.7      1.1     0.4      0.7       0.8      0.1       0.0   2.1      4.3      3.2    269.72            269.51       130    2.075     33M  9155K       0.0       0.0
 Sum     63/12  176.87 MB   0.0      2.4     0.9      1.5       2.6      1.1       0.0   1.2      3.0     18.6    842.41            841.27       320    2.633     72M    10M       0.0      12.7
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      2.4     0.9      1.5       2.1      0.6       0.0   0.0      4.0      3.4    620.27            619.74       167    3.714     72M    10M       0.0       0.0
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.5      0.5       0.0   0.0      0.0     61.1    222.14            221.52       153    1.452       0      0       0.0      12.7

Blob file count: 60, total size: 4.1 GB, garbage size: 0.0 GB, space amp: 1.0


** DB Stats **
Uptime(secs): 1892.6 total, 0.2 interval
Cumulative writes: 18M writes, 18M keys, 18M commit groups, 1.0 writes per commit group, ingest: 14.30 GB, 7.74 MB/s
Cumulative WAL: 18M writes, 0 syncs, 18278197.00 writes per sync, written: 14.30 GB, 7.74 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 3205 writes, 3205 keys, 3161 commit groups, 1.0 writes per commit group, ingest: 2.56 MB, 12.05 MB/s
Interval WAL: 3204 writes, 0 syncs, 3204.00 writes per sync, written: 0.00 GB, 12.05 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               175MB   4GB     15.4    2.0     8.3     853     852     0       13      1778525350.0                                            1893    0.0     0       3.0     0.1     7.9    replay.t1.s1     2024-01-18T11:34:36     8.0.0           41ac5b042d      1.0
```
With 8M more writes which is not acceptable. Let's decrease extra writes to 2M. 
invalid ratio of keys is not so high from LOG.
I may need toadjust lifetime label and put keys that are written once 
into a longer classification.
Let's see how space amp works after I do file deletion.

xingqi's work items
- ssd trace analysis
- ycsba trace analysis 
no model: 10.6% drop
with model: 13% drop
I think we can have more drop keys if 
we test if blob is valid during compaction iter.

[Todo]
Add bytes_read_blob stats in gc job.
```
CompactionJob::Run()
    ProcessKeyValueCompaction()
        compaction iter PrepareOutput
            
          const Status s = blob_fetcher_->FetchBlob(
              user_key(), blob_index, prefetch_buffer, &blob_value_, &bytes_read);

          if (!s.ok()) {
            status_ = s;
            validity_info_.Invalidate();

            return;
          }
        }

        ++iter_stats_.num_blobs_read;
        iter_stats_.total_blob_bytes_read += bytes_read;

        ++iter_stats_.num_blobs_relocated;
        iter_stats_.total_blob_bytes_relocated += blob_index.size();

        
      sub_compact->compaction_job_stats.num_blobs_read =
          c_iter_stats.num_blobs_read;
      sub_compact->compaction_job_stats.total_blob_bytes_read =
          c_iter_stats.total_blob_bytes_read;
      sub_compact->compaction_job_stats.num_input_deletion_records =
          c_iter_stats.num_input_deletion_records;
      sub_compact->compaction_job_stats.num_corrupt_keys =
          c_iter_stats.num_input_corrupt_records;
      sub_compact->compaction_job_stats.num_single_del_fallthru =
          c_iter_stats.num_single_del_fallthru;
 
 // Finish up all book-keeping to unify the subcompaction results
  compact_->AggregateCompactionStats(compaction_stats_, *compaction_job_stats_);
    CompactionJobStats& compaction_job_stats) {
      for (const auto& sc : sub_compact_states) {
        sc.AggregateCompactionStats(compaction_stats);
        compaction_job_stats.Add(sc.compaction_job_stats);
      }
        void CompactionJobStats::Add(const CompactionJobStats& stats) {
          elapsed_micros += stats.elapsed_micros;
          cpu_micros += stats.cpu_micros;

          num_input_records += stats.num_input_records;
          num_blobs_read += stats.num_blobs_read;
          num_input_files += stats.num_input_files;
          num_input_files_at_output_level += stats.num_input_files_at_output_level;

 
  UpdateCompactionStats();
        void CompactionJob::UpdateCompactionStats() {
          assert(compact_);

          Compaction* compaction = compact_->compaction;
          compaction_stats_.stats.num_input_files_in_non_output_levels = 0;
          compaction_stats_.stats.num_input_files_in_output_level = 0;
          for (int input_level = 0;
               input_level < static_cast<int>(compaction->num_input_levels());
               ++input_level) {
            if (compaction->level(input_level) != compaction->output_level()) {
              UpdateCompactionInputStatsHelper(
                  &compaction_stats_.stats.num_input_files_in_non_output_levels,
                  &compaction_stats_.stats.bytes_read_non_output_levels, input_level);
            } else {
              UpdateCompactionInputStatsHelper(
                  &compaction_stats_.stats.num_input_files_in_output_level,
                  &compaction_stats_.stats.bytes_read_output_level, input_level);
            }
          }

          assert(compaction_job_stats_);
          compaction_stats_.stats.bytes_read_blob =
              compaction_job_stats_->total_blob_bytes_read;

          compaction_stats_.stats.num_dropped_records =
              compaction_stats_.DroppedRecords();
        }


```
Difference between CompactionStatsFull and CompactionJobStats?
```
  struct CompactionStatsFull {
    // the stats for the target primary output level
    CompactionStats stats;

    // stats for penultimate level output if exist
    bool has_penultimate_level_output = false;
    CompactionStats penultimate_level_stats;


```
I think I should put bytes read to all lsm-tree compaction log. 
```
Uptime(secs): 1776.9 total, 1.0 interval
Flush(GB): cumulative 6.373, interval 0.000
AddFile(GB): cumulative 0.000, interval 0.000
AddFile(Total Files): cumulative 0, interval 0
AddFile(L0 Files): cumulative 0, interval 0
AddFile(Keys): cumulative 0, interval 0
Cumulative compaction: 28.10 GB write, 16.20 MB/s write, 21.84 GB read, 12.59 MB/s read, 804.2 seconds
I


void InternalStats::DumpCFStatsNoFileHistogram(bool is_periodic,
    void InternalStats::DumpCFMapStats(
        const VersionStorageInfo* vstorage,
        std::map<int, std::map<LevelStatType, double>>* levels_stats,
        CompactionStats* compaction_stats_sum) {
      assert(vstorage);

                    // Stats summary across levels
              std::map<LevelStatType, double> sum_stats;
              PrepareLevelStats(&sum_stats, total_files, total_files_being_compacted,
                                total_file_size, 0, w_amp, *compaction_stats_sum);
              (*levels_stats)[-1] = sum_stats;  //  -1 is for the Sum level
            }


    PrintLevelStats(buf, sizeof(buf), "Sum", levels_stats[-1]);
        void PrintLevelStats(char* buf, size_t len, const std::string& name,
                             const std::map<LevelStatType, double>& stat_value) {
          snprintf(

```

```

Status CompactionJob::Install(const MutableCFOptions& mutable_cf_options) {
  ColumnFamilyData* cfd = compact_->compaction->column_family_data();
  assert(cfd);

  int output_level = compact_->compaction->output_level();
  cfd->internal_stats()->AddCompactionStats(output_level, thread_pri_,
                                            compaction_stats_);


```


```

Status FlushJob::WriteLevel0Table() {
  RecordTimeToHistogram(stats_, FLUSH_TIME, stats.micros);
  cfd_->internal_stats()->AddCompactionStats(0 /* level */, thread_pri_, stats);
  cfd_->internal_stats()->AddCFStats(
      InternalStats::BYTES_FLUSHED,
      stats.bytes_written + stats.bytes_written_blob);
  RecordFlushIOStats();


  void AddCFStats(InternalCFStatsType type, uint64_t value) {
```
I think I only need to add read stats to internal stats


```
Blob file count: 64, total size: 4.6 GB, garbage size: 0.0 GB, space amp: 1.0
```
[Status: Done]

[Todo]
Make gc key insertion with correct semantics. Make sure old keys do not 
overwrite new keys during gc
How does lifetime classification works if put keys back to rocksdb 
after gc?.
We can add a byte of lifetime classification in key during gc put.
So what category of lifetime should we give for each model calling?
We can change model task from classification to regression.
How is data collected?
How does lrb deal with limited amount of training samples and how does
data labelling works for rare seen keys?

```

Status DBImpl::PipelinedWriteImpl(const WriteOptions& write_options,
                                  WriteBatch* my_batch, WriteCallback* callback,
                                  uint64_t* log_used, uint64_t log_ref,
                                  bool disable_memtable, uint64_t* seq_used) {
    memtable_write_group.status = WriteBatchInternal::InsertInto(
          memtable_write_group, w.sequence, column_family_memtables_.get(),
          &flush_scheduler_, &trim_history_scheduler_,
          write_options.ignore_missing_column_families, 0 /*log_number*/, this,
          false /*concurrent_memtable_writes*/, seq_per_batch_, batch_per_txn_);


                w->status = w->batch->Iterate(&inserter);
```

```
Status WriteBatchInternal::Put(WriteBatch* b, uint32_t column_family_id,
                               const Slice& key, const Slice& value) {
  if (key.size() > size_t{std::numeric_limits<uint32_t>::max()}) {
    return Status::InvalidArgument("key is too large");
  }
  if (value.size() > size_t{std::numeric_limits<uint32_t>::max()}) {
    return Status::InvalidArgument("value is too large");
  }

  LocalSavePoint save(b);
  WriteBatchInternal::SetCount(b, WriteBatchInternal::Count(b) + 1);
  if (column_family_id == 0) {
    b->rep_.push_back(static_cast<char>(kTypeValue));
  } else {
 
```
Where is tag is set during write?
I think we can add an another type of op in WriteBatchInternal 
which is call PutGC()
a: 3
put(a,3)
put(a,4)
get(a) ->  3 / 4

Sacarifice read performance. Read all levels to get latest value.

Actually there is PutBlobValue() in rocksdb which is similar to gc put
but is different from normal put in current integrated blob db in rocksdb 
which accumulated value in memtable and then flush value to blob file.
```
Status WriteBatchInternal::InsertInto(
    WriteThread::WriteGroup& write_group, SequenceNumber sequence,
    ColumnFamilyMemTables* memtables, FlushScheduler* flush_scheduler,
    TrimHistoryScheduler* trim_history_scheduler,
    bool ignore_missing_column_families, uint64_t recovery_log_number, DB* db,
    bool concurrent_memtable_writes, bool seq_per_batch, bool batch_per_txn) {
 
Status WriteBatch::Iterate(Handler* handler) const {
  if (rep_.size() < WriteBatchInternal::kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }

  return WriteBatchInternal::Iterate(this, handler, WriteBatchInternal::kHeader,
                                     rep_.size());
}
    Status WriteBatchInternal::Iterate(const WriteBatch* wb,
                                       WriteBatch::Handler* handler, size_t begin,
                                       size_t end) {
      if (begin > wb->rep_.size() || end > wb->rep_.size() || end < begin) {
        return Status::Corruption("Invalid start/end bounds for Iterate");
      }
      assert(begin <= end);
      Slice input(wb->rep_.data() + begin, static_cast<size_t>(end - begin));
      bool whole_batch =
          (begin == WriteBatchInternal::kHeader) && (end == wb->rep_.size());

      Slice key, value, blob, xid;

     
        switch (tag) {
          case kTypeColumnFamilyValue:
          case kTypeValue:
            assert(wb->content_flags_.load(std::memory_order_relaxed) &
                   (ContentFlags::DEFERRED | ContentFlags::HAS_PUT));
            s = handler->PutCF(column_family, key, value);

            // s = handler->PutCFWithFeatures(column_family, key, value );
            if (LIKELY(s.ok())) {
              empty_batch = false;
              found++;

  Status PutCF(uint32_t column_family_id, const Slice& key,
               const Slice& value) override {
    const auto* kv_prot_info = NextProtectionInfo();
    Status ret_status;
    if (kv_prot_info != nullptr) {
      assert(false);
      // Memtable needs seqno, doesn't need CF ID
      auto mem_kv_prot_info =
          kv_prot_info->StripC(column_family_id).ProtectS(sequence_);
      ret_status = PutCFImpl(column_family_id, key, value, kTypeValue,
                             &mem_kv_prot_info);
    } else {
      ret_status = PutCFImplWithFeatures(column_family_id, key, value, kTypeValue,
                                         nullptr);

```
Handler shares PutCF() interface
So I need to implement PutCF() of MemTableInserter
Implement WriteBatch::PutGCCF()
Implement WriteBatchInternal:PutGCCF()
I think I can just call PutBlobIndexCF of memtable inserter.
I don't need to do that myself by implementing a new type.
But I need to extract sequence number out of key.

Should change value type from kblob to ktypevalue

DEFINE_bool(allow_concurrent_memtable_write, true,

```
Status DBImpl::Write(const WriteOptions& write_options, WriteBatch* my_batch) {
  Status s;
  if (write_options.protection_bytes_per_key > 0) {
    s = WriteBatchInternal::UpdateProtectionInfo(
        my_batch, write_options.protection_bytes_per_key);
  }
  if (s.ok()) {
    s = WriteImpl(write_options, my_batch, /*callback=*/nullptr,
                  /*log_used=*/nullptr);
  }
  return s;
}

```
Change user_key() to key() in ExtractLargeValueIfNeededImpl() to get 
internal key so that we can put sequence number during gc.
We can call this function to write writebatch into rocksdb

Another run before use PutGC
```

Uptime(secs): 1891.3 total, 8.0 interval
Flush(GB): cumulative 13.293, interval 0.092
AddFile(GB): cumulative 0.000, interval 0.000
AddFile(Total Files): cumulative 0, interval 0
AddFile(L0 Files): cumulative 0, interval 0
AddFile(Keys): cumulative 0, interval 0
Cumulative compaction: 15.34 GB write, 8.31 MB/s write, 2.42 GB read, 1.31 MB/s read, 821.4 seconds
Interval compaction: 0.09 GB write, 11.73 MB/s write, 0.00 GB read, 0.00 MB/s read, 1.3 seconds
Stalls(count): 0 level0_slowdown, 0 level0_slowdown_with_compaction, 0 level0_numfiles, 0 level0_numfiles_with_compaction, 0 stop for pending_compaction_bytes, 0 slowdown for pending_compaction_bytes, 0 memtable_compaction, 0 memtable_slowdown, interval 0 total count
Block cache LRUCache@0x5555569d0d60#759148 capacity: 16.00 GB usage: 2.43 GB table_size: 1048576 occupancy: 41920 collections: 11 last_copies: 85 last_secs: 0.404451 secs_since: 85
Block cache entry stats(count,size,portion): DataBlock(267213,2.06 GB,12.8446%) BlobValue(405704,329.34 MB,2.01012%) Misc(2,8.09 KB,4.82425e-05%)

** File Read Latency Histogram By Level [default] **
2024/01/24-15:08:57.525420 759153 [db/compaction/compaction_job.cc:1666] [default] [JOB 398] Generated table #1218: 74006 keys, 2675662 bytes, temperature: kUnknown
2024/01/24-15:08:57.525471 759153 EVENT_LOG_v1 {"time_micros": 1706080137525445, "cf_name": "default", "job": 398, "event": "table_file_creation", "file_number": 1218, "file_size": 2675662, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 15255519, "largest_seqno": 18104087, "table_properties": {"data_size": 2576002, "index_size": 6047, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 92549, "raw_key_size": 2072168, "raw_average_key_size": 28, "raw_value_size": 740060, "raw_average_value_size": 10, "num_data_blocks": 316, "num_entries": 74006, "num_filter_entries": 74006, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1706079397, "oldest_key_time": 0, "file_creation_time": 1706080136, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "3dca4e63-ea19-41a5-bb3b-c3fd14b447cd", "db_session_id": "R9V51Z2GXOEEUD257B86", "orig_file_number": 1218, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 1007}
2024/01/24-15:08:57.859432 759182 [db/db_impl/db_impl.cc:1123] ------- DUMPING STATS -------
2024/01/24-15:08:57.859505 759182 [db/db_impl/db_impl.cc:1125] 
** DB Stats **
Uptime(secs): 1892.3 total, 1.0 interval
Cumulative writes: 18M writes, 18M keys, 17M commit groups, 1.0 writes per commit group, ingest: 14.20 GB, 7.68 MB/s
Cumulative WAL: 18M writes, 0 syncs, 18152106.00 writes per sync, written: 14.20 GB, 7.68 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 14K writes, 14K keys, 14K commit groups, 1.0 writes per commit group, ingest: 11.51 MB, 11.50 MB/s
Interval WAL: 14K writes, 0 syncs, 14422.00 writes per sync, written: 0.01 GB, 11.50 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      4/4   15.27 MB   0.0      0.0     0.0      0.0       0.5      0.5       0.0   1.0      0.0     61.8    220.37            219.67       152    1.450       0      0       0.0      12.8
  L1      8/8   23.79 MB   0.0      1.3     0.5      0.8       1.2      0.5       0.0   2.3      3.9      3.7    340.06            339.65        37    9.191     38M  1756K       0.0       0.0
  L2     49/0   133.15 MB   0.7      1.1     0.4      0.7       0.8      0.1       0.0   2.0      4.4      3.2    260.97            260.69       127    2.055     33M  9185K       0.0       0.0
 Sum     61/12  172.22 MB   0.0     11.2     0.9      1.5       2.6      1.1       0.0   1.2     13.9     19.1    821.40            820.02       316    2.599     71M    10M       8.8      12.8
 Int      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0     11.2     0.9      1.5       8.3      6.8       0.0   0.0     19.0     14.1    601.03            600.35       164    3.665     82M    13M       8.8       0.0
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.5      0.5       0.0   0.0      0.0     61.8    220.37            219.67       152    1.450       0      0       0.0      12.8

Blob file count: 52, total size: 3.7 GB, garbage size: 0.0 GB, space amp: 1.0


Completed replay (ID: ) in 2451 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50     p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test  date     version job_id  githash
0               172MB   4GB     15.3    2.0     8.3     821     820     9       13      1778525635.0                                           1895    0.0     0       2.9     0.1     8.2   replay.t1.s1     2024-01-24T14:39:22     8.0.0           41ac5b042d      1.0
```

Get a bug where get report blob:15 not found , blob:15 is deleted. 
Is this because get seearch cache first and get obsolete value for key ?
How can we clear cache after gc job?
How does compaction handle this?
Is this also becase blob files are deleted while get is ongoing after gettting
keys from lsm-tree?
To avoid this problem we can ref blob  in version as well.
1. each FileMetaData has ref count. FileMetaData is deleted only when ref = 0;
2. TableCache is clear before FileMetaData ref--? Otherwise get will still get 
old sst from cache. Actually we don't need to clear cache for deleted sst.
Because get start from each version.
```
void SubcompactionState::Cleanup(Cache* cache) {
  penultimate_level_outputs_.Cleanup();
  compaction_outputs_.Cleanup();

  if (!status.ok()) {
    for (const auto& out : GetOutputs()) {
      // If this file was inserted into the table cache then remove
      // them here because this compaction was not committed.
      TableCache::Evict(cache, out.meta.fd.GetNumber());
    }
  }
  // TODO: sub_compact.io_status is not checked like status. Not sure if thats
  // intentional. So ignoring the io_status as of now.
  io_status.PermitUncheckedError();
}
```

ref--
```
Version::~Version() {
  assert(refs_ == 0);

  // Remove from linked list
  prev_->next_ = next_;
  next_->prev_ = prev_;

  // Drop references to files
  for (int level = 0; level < storage_info_.num_levels_; level++) {
    for (size_t i = 0; i < storage_info_.files_[level].size(); i++) {
      FileMetaData* f = storage_info_.files_[level][i];
      assert(f->refs > 0);
      f->refs--;
      if (f->refs <= 0) {
        assert(cfd_ != nullptr);
        uint32_t path_id = f->fd.GetPathId();
        assert(path_id < cfd_->ioptions()->cf_paths.size());
        vset_->obsolete_files_.push_back(
            ObsoleteFileInfo(f, cfd_->ioptions()->cf_paths[path_id].path,
                             cfd_->GetFileMetadataCacheReservationManager()));
      }
    }
  }
}


  void UnrefFile(FileMetaData* f) {
    f->refs--;
    if (f->refs <= 0) {
      if (f->table_reader_handle) {
        assert(table_cache_ != nullptr);
        // NOTE: have to release in raw cache interface to avoid using a
        // TypedHandle for FileMetaData::table_reader_handle
        table_cache_->get_cache().get()->Release(f->table_reader_handle);
        f->table_reader_handle = nullptr;
      }



```

ref++
```
  Status ApplyFileAddition(int level, const FileMetaData& meta) {
    assert(level != VersionStorageInfo::FileLocation::Invalid().GetLevel());

    const uint64_t file_number = meta.fd.GetNumber();

    const int current_level = GetCurrentLevelForTableFile(file_number);

    if (current_level !=
 
    FileMetaData* const f = new FileMetaData(meta);
    f->refs = 1;


void VersionStorageInfo::AddFile(int level, FileMetaData* f) {
  auto& level_files = files_[level];
 level_files.push_back(f);

  f->refs++;
}
```
So I will need t add ref to blobfilemetadata and unref then at the destructor 
of eahc version.
I don't know where to put refs. SharedBlobFileMetaData or blobfilemetadatas?  
Should change shared_ptr to std ptr of blobfile?
Don't think this ref of blob file will work because
current  implementation of version already hold refs to blob file meta .
But this is weird. Why blob 15 is searched?
Maybe there is something wrong with PutGC.
There is no valid_key_num log in LOG. so there must be something 
wrong with PutGC.

Realize that I might not need to implement my own gc process.
I  can just update gc process to check validity of keys and check
if blob index points to blob that is in current gc.
```
(gdb) p s.ToString()
$1 = "Corruption: unknown WriteBatch tag"
```

Still get blob not existed after I fix PutGC()
Is there is error in PutGC()?
No such issue after I replace PutGC() with Put()

Test WriteBatch PutGC()

[Status: Abandoned]

[Todo]
Subtract write bytes during flush for keys written in gc.
This gives us higher and accurate w-amp.
Current w-amp is 1.2. 
It should be higher.
[Status: Not started]

[Todo]
Add comp cpu stats?
internal_stats.cc
```
PrintLevelStatsHeader(buf, sizeof(buf), cfd_->GetName(), "Priority");
  value->append(buf);
  std::map<int, std::map<LevelStatType, double>> priorities_stats;
  DumpCFMapStatsByPriority(&priorities_stats);
  for (size_t priority = 0; priority < comp_stats_by_pri_.size(); ++priority) {
    if (priorities_stats.find(static_cast<int>(priority)) !=
        priorities_stats.end()) {
      PrintLevelStats(
          buf, sizeof(buf),
          Env::PriorityToString(static_cast<Env::Priority>(priority)),
          priorities_stats[static_cast<int>(priority)]);
      value->append(buf);
    }
  }


```
[Status: Not started]

[Todo]
Cache level similar for key feature generation for GC.

[Status: Not started]

[Todo]
set allow_concurrent_memtable_write to  false?
But we only have one write thread so I think this is ok for now.
[Status: Not started]


[Todo]
We can increase lsm-tree key drop if we check blob file existence during compaction.
[Status: Not started]


[Todo]
Set GC job thread priority to low
[Status: Not started]


- No current gc in compaction_iterator.
- Write a periodic GC function called in BackgroundCompaction()
- Pick files in GC function 
    Pick Blob files, 
    Iterate blob files
    Check validity of key and value.
        Compare value of key in internal iterator.
        I can compare value in value file or I can just compare 
        sequence number. If there n
    Write back valid keys.
    Delete old blob files.
[Status: Done]


[Todo]
No blob file deletion why is that?
Add log to LOG .
I think this is because blob files do not have all linked files
which make sst scanning not able to gc all values in blob files.
There are deleted blob files log after should_gc logic change.
```
    if(compaction_ && 
      compaction_->real_compaction()->compaction_reason() == CompactionReason::kForcedBlobGC ) {

      should_gc = true;

    }


    if(compaction_ && gc_blob_files_.find(blob_index.file_number()) != gc_blob_files_.end()) {
      should_gc = true;
    }


```


```
Status FileMetaData::UpdateBoundaries(const Slice& key, const Slice& value,
                                      SequenceNumber seqno,
                                      ValueType value_type) {
  if (value_type == kTypeBlobIndex) {
    BlobIndex blob_index;
    const Status s = blob_index.DecodeFrom(value);
    if (!s.ok()) {
      return s;
    }

    if (!blob_index.IsInlined() && !blob_index.HasTTL()) {
      if (blob_index.file_number() == kInvalidBlobFileNumber) {
        return Status::Corruption("Invalid blob file number");
      }

      if (oldest_blob_file_number == kInvalidBlobFileNumber ||
          oldest_blob_file_number > blob_index.file_number()) {
        oldest_blob_file_number = blob_index.file_number();
      }
    }

```

Only one sst files for one gc compaction job which is not right.
No blob file deletion.
```
2024/01/27-13:20:31.164418 3163386 EVENT_LOG_v1 {"time_micros": 1706332831164408, "job": 183, "event": "compaction_started", "compaction_reason": "ForcedBlobGC", "files_L2": [315], "gc_blob_files": [11, 15, 19, 23, 31, 35, 39, 43, 54, 58, 62, 66, 80, 84, 88], "score": 0.943997, "input_data_size": 3411686, "oldest_snapshot_seqno": -1}
```

```
// Forced blob garbage collection
  PickFileToCompact(vstorage_->FilesMarkedForForcedBlobGC(), false);
  if (!start_level_inputs_.empty()) {
    compaction_reason_ = CompactionReason::kForcedBlobGC;
    return;
  }
```

Not all sst files are used in one compaction.
```
2024/01/27-16:17:57.751443 3246585 [db/version_set.cc:3405] Files marked for forced blob gc: 94,75,74,73,72,71, | 11,
2024/01/27-16:17:57.751481 3246585 [db/compaction/compaction_job.cc:2082] [default] [JOB 27] Compacting 1@1 files to L1, score 0.75
2024/01/27-16:17:57.751487 3246585 [db/compaction/compaction_job.cc:2088] [default]: Compaction start summary: Base version 27 Base level 1, inputs: [94(1431KB)]
2024/01/27-16:17:57.751499 3246585 EVENT_LOG_v1 {"time_micros": 1706343477751491, "job": 27, "event": "compaction_started", "compaction_reason": "ForcedBlobGC", "files_L1": [94], "gc_blob_files": [11], "score": 0.75, "input_data_size": 1466301, "oldest_snapshot_seqno": -1}
2024/01/27-16:17:57.751570 3246585 [db/compaction/compaction_iterator.cc:228]
```

```
2024/01/27-16:17:58.107275 3246585 [db/version_set.cc:3405] Files marked for forced blob gc: 95,75,74,73,72,71, | 11,
2024/01/27-16:17:58.460761 3246585 [db/version_set.cc:3405] Files marked for forced blob gc: 96,75,74,73,72,71, | 11,
```
Why sst:96 points to blob:11 ? This should not happen.
Always newest sst files are picked to start gc.
Adjust code to pick oldest sst files to start gc.

How does inflow and out flow works for each gc?

Need to fix tiemstamp issue in this branch. No need to do that . It's because blob:11 is not deleted.

```
  void Next() override {
    assert(Valid());

    iter_->Next();
    UpdateAndCountBlobIfNeeded();
  }


  void UpdateAndCountBlobIfNeeded() {
    assert(!iter_->Valid() || iter_->status().ok());

    if (!iter_->Valid()) {
      status_ = iter_->status();
      return;
    }

    TEST_SYNC_POINT(
        "BlobCountingIterator::UpdateAndCountBlobIfNeeded:ProcessInFlow");

    status_ = blob_garbage_meter_->ProcessInFlow(key(), value());
  }


```

```
Status CompactionOutputs::AddToOutput(
    const CompactionIterator& c_iter,
    const CompactionFileOpenFunc& open_file_func,
    const CompactionFileCloseFunc& close_file_func) {
 
      if (blob_garbage_meter_) {
        s = blob_garbage_meter_->ProcessOutFlow(key, value);
      }

Status BlobGarbageMeter::ProcessOutFlow(const Slice& key, const Slice& value) {
  uint64_t blob_file_number = kInvalidBlobFileNumber;
  uint64_t bytes = 0;

  const Status s = Parse(key, value, &blob_file_number, &bytes);
  if (!s.ok()) {
    return s;
  }

  if (blob_file_number == kInvalidBlobFileNumber) {
    return Status::OK();
  }

  // Note: in order to measure the amount of additional garbage, we only need to
  // track the outflow for preexisting files, i.e. those that also had inflow.
  // (Newly written files would only have outflow.)
  auto it = flows_.find(blob_file_number);
  if (it == flows_.end()) {
    return Status::OK();
  }

  it->second.AddOutFlow(bytes);

  return Status::OK();
}
```


Turns out I didn't SetGCBlobFiles() lol

No blob file deletion after linkedssts are deleted which is not what I expect.
blob:11 still has valid blobs even though no linkedssts.
Is this because my code linking sst has bugs?
```
2024/01/27-19:00:55.193167 3325205 [db/blob/blob_file_builder.cc:425] [default] [JOB 2] Generated blob file #11: 2840 total blobs, 2491428 total bytes, lifetime label: 0
```
Is this because of that some keys are dropped? I don't think so. dropped keys are also count as garbage.

```
  for (const auto& pair : blob_total_garbage) {
    const uint64_t blob_file_number = pair.first;
    const BlobGarbageMeter::BlobStats& stats = pair.second;

    edit->AddBlobFileGarbage(blob_file_number, stats.GetCount(),
                             stats.GetBytes());
  }


```
How does blob_garbage_meter deal with dropped keys?
Exclude possible reasons for dropped key count.

I think I found the bug.
There is a trivial move in LOG and I checked compaction code 
and found that I didn't put f->linked_blob_files to c->edit()->AddFiles()
which causes the some ssts are unlinked from blob files.
Finally fixed the bug after on day work.
[Status: Done]


Too many ssts to be used in compaction for gc
```
2024/01/27-21:47:52.555034 3412129 [db/version_set.cc:3405] Files marked for forced blob gc: 684,680,692,682,708,710,706,702,714,694,672,686,696,670,724,722,720,718,716,688,674,726,668,704,730, | 228,230,232,234,236,238,240,243,247,249,251,253,255,257,261,265,
```

```
Uptime(secs): 1883.6 total, 1.0 interval
Cumulative writes: 9957K writes, 9957K keys, 9957K commit groups, 1.0 writes per commit group, ingest: 7.85 GB, 4.27 MB/s
Cumulative WAL: 9957K writes, 0 syncs, 9957298.00 writes per sync, written: 7.85 GB, 4.27 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 5704 writes, 5704 keys, 5704 commit groups, 1.0 writes per commit group, ingest: 4.60 MB, 4.59 MB/s
Interval WAL: 5704 writes, 0 syncs, 5704.00 writes per sync, written: 0.00 GB, 4.59 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.4    245.23            244.80        85    2.885       0      0       0.0       6.1
  L1      8/0   22.05 MB   0.9      1.6     0.3      1.0       1.2      0.2       0.0   2.4      4.6      4.4    362.78            362.25       191    1.899     36M  1535K       0.4       0.4
  L2     96/1   126.84 MB   0.6      7.1     0.1      3.0       3.0      0.1       0.0   1.7      7.9      7.8    923.02            921.00      1577    0.585     93M  1672K       4.0       4.0
 Sum    105/1   152.04 MB   0.0      8.7     0.4      3.9       4.5      0.6       0.0   2.4      5.9     10.0   1531.03           1528.04      1853    0.826    129M  3207K       4.4      10.5
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0      0.0       0.0 113456008.0     10.9     10.7     10.11             10.09        12    0.843    907K    56K       0.1       0.1

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      8.7     0.4      3.9       4.2      0.3       0.0   0.0      7.0      6.9   1285.79           1283.24      1768    0.727    129M  3207K       4.4       4.4
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.4    245.23            244.80        85    2.885       0      0       0.0       6.1

Blob file count: 572, total size: 4.6 GB, garbage size: 1.1 GB, space amp: 1.3



blob_space_amps: 1.3
dir_size_without_trace is 4.8, cumulative writes 7.9
space_amp is 0.6
report file name is /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_model_gc_1.0_0.8/report.tsv
Completed replay (ID: ) in 2041 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               152MB   4GB     15.1    2.7     8.2     1539    1536    4       11      1778525347.0                                            1892    0.0     0       2.7     0.1     7.4    replay.t1.s1     2024-01-27T21:36:03     8.0.0           d975024386      0.6
```
[Todo]
Update condition to that doing gc as long as gc_blob_files_ is not empty without regarding
compaction reason.
Not good. 
```
  with_model_gc_1.0_0.8 grep blob_file_deletion LOG | wc -l
1350


2024/01/27-22:44:01.444088 3434500 [DEBUG] [db/db_impl/db_impl_files.cc:367] [JOB 1807] Delete /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_model_gc_1.0_0.8/004198.blob type=10 #4198 -- OK
```

```
Uptime(secs): 1891.5 total, 1.0 interval
Cumulative writes: 10M writes, 10M keys, 10M commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 4.27 MB/s
Cumulative WAL: 10M writes, 0 syncs, 10000335.00 writes per sync, written: 7.88 GB, 4.27 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 5522 writes, 5522 keys, 5522 commit groups, 1.0 writes per commit group, ingest: 4.46 MB, 4.46 MB/s
Interval WAL: 5523 writes, 0 syncs, 5523.00 writes per sync, written: 0.00 GB, 4.46 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.2    247.34            247.00        85    2.910       0      0       0.0       6.1
  L1      9/0   24.48 MB   1.0      1.7     0.3      1.0       1.2      0.2       0.0   2.3      4.7      4.6    363.90            363.47       187    1.946     36M  1542K       0.5       0.5
  L2    102/1   125.65 MB   0.6      7.3     0.1      3.0       3.1      0.1       0.0   1.7      8.1      8.1    924.84            923.18      1517    0.610     94M  1628K       4.2       4.2
 Sum    112/1   153.28 MB   0.0      9.0     0.4      3.9       4.5      0.5       0.0   2.4      6.0     10.2   1536.08           1533.65      1789    0.859    130M  3171K       4.7      10.7
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0      0.0       0.0 117112462.0     15.5     15.5      7.22              7.20        22    0.328    598K      0       0.1       0.1

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      9.0     0.4      3.9       4.2      0.3       0.0   0.0      7.2      7.1   1288.74           1286.66      1704    0.756    130M  3171K       4.7       4.7
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.2    247.34            247.00        85    2.910       0      0       0.0       6.1

Blob file count: 518, total size: 4.4 GB, garbage size: 0.9 GB, space amp: 1.2


```
Consider invoke filter to do internaliterator:get to see if we can drop key.

```
class FilterByKeyLength : public CompactionFilter {
 public:
  explicit FilterByKeyLength(size_t len) : length_threshold_(len) {}
  const char* Name() const override {
    return "rocksdb.compaction.filter.by.key.length";
  }
  CompactionFilter::Decision FilterBlobByKey(
      int /*level*/, const Slice& key, std::string* /*new_value*/,
      std::string* /*skip_until*/) const override {
    if (key.size() < length_threshold_) {
      return CompactionFilter::Decision::kRemove;
    }
    return CompactionFilter::Decision::kKeep;
  }

 private:
  size_t length_threshold_;
};


ImmutableCFOptions::ImmutableCFOptions(const ColumnFamilyOptions& cf_options)
    : compaction_style(cf_options.compaction_style),
      compaction_pri(cf_options.compaction_pri),
      user_comparator(cf_options.comparator),
      internal_comparator(InternalKeyComparator(cf_options.comparator)),
      merge_operator(cf_options.merge_operator),
      compaction_filter(cf_options.compaction_filter),
      compaction_filter_factory(cf_options.compaction_filter_factory),


```

```

std::unique_ptr<CompactionFilter>
TtlCompactionFilterFactory::CreateCompactionFilter(
    const CompactionFilter::Context& context) {
  std::unique_ptr<const CompactionFilter> user_comp_filter_from_factory =
      nullptr;
  if (user_comp_filter_factory_) {
    user_comp_filter_from_factory =
        user_comp_filter_factory_->CreateCompactionFilter(context);
  }

  return std::unique_ptr<TtlCompactionFilter>(new TtlCompactionFilter(
      ttl_, clock_, nullptr, std::move(user_comp_filter_from_factory)));
}


```

```
  if (filter == CompactionFilter::Decision::kRemove) {
    // convert the current key to a delete; key_ is pointing into
    // current_key_ at this point, so updating current_key_ updates key()
    ikey_.type = kTypeDeletion;
    current_key_.UpdateInternalKey(ikey_.sequence, kTypeDeletion);
    // no value associated with delete
    value_.clear();
    iter_stats_.num_record_drop_user++;
  } else if (filter == CompactionFilter::Decision::kPurge) {

```
0.023 drop rate

Added obsolete key checking for gc blob file keys. 
0.028 drop rate 
```

Cumulative writes: 9997K writes, 9997K keys, 9997K commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 4.26 MB/s
Cumulative WAL: 9997K writes, 0 syncs, 9997567.00 writes per sync, written: 7.88 GB, 4.26 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 5647 writes, 5647 keys, 5647 commit groups, 1.0 writes per commit group, ingest: 4.56 MB, 4.56 MB/s
Interval WAL: 5647 writes, 0 syncs, 5647.00 writes per sync, written: 0.00 GB, 4.56 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    3.15 MB   0.2      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     26.7    242.60            242.21        85    2.854       0      0       0.0       6.1
  L1      7/0   22.58 MB   0.9      1.5     0.3      0.8       1.0      0.2       0.0   2.1      4.2      4.0    366.25            365.88       145    2.526     31M  1530K       0.4       0.4
  L2     47/1   120.65 MB   0.6      5.7     0.1      2.3       2.4      0.1       0.0   1.7      6.1      6.0    963.14            961.85       862    1.117     74M  1844K       3.2       3.2
 Sum     55/1   146.38 MB   0.0      7.2     0.4      3.1       3.7      0.5       0.0   2.1      4.7      8.7   1571.99           1569.94      1092    1.440    106M  3374K       3.7       9.7
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0     -0.0       0.0 91875142.0      9.7      9.6      9.15              9.14         6    1.525    369K    23K       0.1       0.1

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      7.2     0.4      3.1       3.4      0.3       0.0   0.0      5.6      5.5   1329.38           1327.73      1007    1.320    106M  3374K       3.7       3.7
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     26.7    242.60            242.21        85    2.854       0      0       0.0       6.1


Blob file count: 246, total size: 4.1 GB, garbage size: 0.8 GB, space amp: 1.2
```
standard rocksdb -> full feature lifetime gc -> online model lifetime gc(not good) -> normal model(sample not good ) -> online model feature in lsm-tree value, default 0 lifetime -> online model feature in lsm-tree value, default 1 lifetime -> online model feature in lsm-tree value, default 1 lifetime with dedicated gc job for blob files(too many sst), write count during gc is not counted yet -> online model, dedicated gc job, write count is counted, model is not good -> fix model prediction bug, 100k number of samples triggers model train, default-1 -> fix model , 100k trigger model training, default-1, blob index update during compaction, blob file map and blob offset map deletion after gc -> regression model[1M, 2M, 4M, 8M] -> regression model[1M,2M,4M,8M] with value moving to longer lifetime bucket during gc -> regression model with inverse proportional of past distances count training data add -> no model
```
w-amp: 4.4 -> 2.1 -> 3.3 -> 2.9 ->  3.1 -> 2.5 -> 1.2 -> 1.7 -> 1.8 -> 1.8 -> 2.3 -> 1.9 ->  1.9 -> 1.9
space-amp: 1.2 -> 1.2 -> 1.3 -> 1.2 -> 1.3 -> 1.2 -> 1.0 -> 1.0 -> 1.0 -> 1.0 -> 1.0 -> 1.0 -> 1.0 -> 1.0
blob total size:  4.3 -> 4.1 -> 4.0 -> 4.0 -> 4.4 -> 4.2 -> 4.1 -> 4.1 -> 3.8 -> 3.8 -> 3.6 -> 3.9 -> 3.8 GB -> 3.9GB
Read: 21.8GB -> 7.2 GB -> 14.9 GB -> 10.2GB -> 11.4 GB -> 7.1 GB -> 4.8 GB -> 6.8GB -> 7.8 GB -> 7.8GB -> 10.9 GB -> 8.3 GB -> 8.3 GB -> 8.0GB
Write: 1.1 GB -> 3.7 GB ->  2.0 GB -> 1.9GB -> 2.7 GB -> 4.1 GB -> 4.4 GB -> 1.9 GB -> 1.9 GB -> 1.9 GB -> 1.9 GB -> 1.9 GB -> 1.9 GB -> 1.9 GB
Read blob: 20.8 GB -> 3.7 GB -> 12.8 GB -> 8.4 GB -> 8.9 GB -> 3.3 GB -> 3.2 GB -> 5.2 GB -> 6.1GB -> 6.1 GB -> 9.2 GB -> 6.6 GB -> 6.7 GB -> 6.3 GB
Write blob: 27 GB  -> 9.7 GB -> 12.8 GB -> 14.4 GB -> 15.0 GB -> 9.3 GB -> 6.1 GB -> 9.3 GB -> 9.9 GB -> 9.9 GB -> 12.8 GB -> 10.5 GB -> 10.5 GB -> 10.2 GB
garbage size: 0.6 GB -> x GB ->  x GB -> x GB -> 1.0 GB ->  0.8 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB -> 0.0 GB

Cumulative write rate: 4.54 MB/s -> 4.26 MB/s -> 3.85 MB/s -> 4.08 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s -> 4.54 MB/s
Uptime(secs): 1777.9 total, 1.0 interval
 -> Uptime(secs): 1891.7 total, 8.0 interval
 -> Uptime(secs): 2094.8 total, 1.0 interval
 -> Uptime(secs): 1974.1 total, 8.0 interval
 -> Uptime(secs): 1777.4 total, 1.0 interval
 -> Uptime(secs): 1776.5 total, 1.0 interval
 -> Uptime(secs): 1777.9 total, 1.0 interval
 -> Uptime(secs): 1773.8 total, 1.0 interval
 -> Uptime(secs): 1777.0 total, 1.0 interval
 -> Uptime(secs): 1778.1 total, 1.0 interval
 -> Uptime(secs): 1773.1 total, 1.0 interval
 -> Uptime(secs): 1773.1 total, 1.0 interval
 -> Uptime(secs): 1772.1 total, 1.0 interval
 -> Uptime(secs): 1755.2 total, 1.0 interval
```
 std rocksdb gc: Cumulative compaction: 28.10 GB write, 16.20 MB/s write, 21.84 GB read, 12.59 MB/s read, 804.2 seconds
 ->
lifetime blob gc: Cumulative compaction: 15.23 GB write, 8.24 MB/s write, 9.02 GB read, 4.88 MB/s read, 1536.1 seconds
->
lifetime blob gc with obsolete key checking during gc: Cumulative compaction: 13.42 GB write, 7.27 MB/s write, 7.22 GB read, 3.91 MB/s read, 1572.0 seconds
-> 
(problematic) online model training lifetime blob gc with obsolete key checking Cumulative compaction: 21.10 GB write, 10.31 MB/s write, 14.90 GB read, 7.28 MB/s read, 1730.7 seconds
-> 
online model with obsolete key checking(working model ) :Cumulative compaction: 16.37 GB write, 8.49 MB/s write, 10.17 GB read, 5.27 MB/s read, 2743.2 seconds
->
online model with key meta stored in lsm-tree value， default 0 lifetime:  Cumulative compaction: 17.72 GB write, 10.21 MB/s write, 11.41 GB read, 6.58 MB/s read, 2804.5 seconds
->
online model with key meta stored in lsm-tree value， default 1 lifetime: Cumulative compaction: 13.43 GB write, 7.74 MB/s write, 7.14 GB read, 4.11 MB/s read, 2378.3 seconds
->
online model feature in lsm-tree value, default 1 lifetime with dedicated gc job for blob files(too many sst), write count during gc is not counted yet : Cumulative compaction: 7.94 GB write, 4.59 MB/s write, 1.61 GB read, 0.93 MB/s read, 1500.9 seconds
->
online model, dedicated gc job, write count is counted, model is not good:  Cumulative compaction: 7.96 GB write, 4.59 MB/s write, 1.63 GB read, 0.94 MB/s read, 1497.9 seconds
->
fix model prediction bug, default-1: Cumulative compaction: 8.01 GB write, 4.62 MB/s write, 1.69 GB read, 0.97 MB/s read, 1161.6 seconds
->
fix model , 100k trigger model training, default-1, blob index update during compaction, blob file map and blob offset map deletion after gc:  Cumulative compaction: 8.01 GB write, 4.62 MB/s write, 1.69 GB read, 0.97 MB/s read, 1195.7 seconds
->
 regression model: Cumulative compaction: 8.00 GB write, 4.62 MB/s write, 1.68 GB read, 0.97 MB/s read, 980.1 seconds
->
regression model[1M,2M,4M,8M] with value moving to longer lifetime bucket during gc: Cumulative compaction: 7.99 GB write, 4.62 MB/s write, 1.66 GB read, 0.96 MB/s read, 970.1 seconds
-> 
regression model with inverse proportional of past distances count training data add: Cumulative compaction: 8.00 GB write, 4.62 MB/s write, 1.67 GB read, 0.97 MB/s read, 872.3 seconds
->
no model: Cumulative compaction: 7.93 GB write, 4.63 MB/s write, 1.68 GB read, 0.98 MB/s read, 613.4 seconds





with_model_wisckey_style_gc:
```
 Cumulative compaction: 15.53 GB write, 8.42 MB/s write, 2.47 GB read, 1.34 MB/s read, 843.9 seconds

Cumulative writes: 18M writes, 18M keys, 18M commit groups, 1.0 writes per commit group, ingest: 14.38 GB, 7.78 MB/s
```
[Status: Done]


[Todo]
Build key features collection module
Sequence number is in Writer.
We need sequence number to keep track of time and update past distnace of KeyMeta.
I need a way to prepare sparsity matrix just like what lrb do.
It's very nasty to prepare sparsity matrix. 
But I think I need to do that.
Currently I read features I generated from python script and load it
via cpp into vectors.
I need a way to record keys collected during rocksdb running.
```
  struct Writer {
    WriteBatch* batch;
    bool sync;
    bool no_slowdown;
    bool disable_wal;
    Env::IOPriority rate_limiter_priority;
    bool disable_memtable;
    size_t batch_cnt;  // if non-zero, number of sub-batches in the write batch
    size_t protection_bytes_per_key;
    PreReleaseCallback* pre_release_callback;
    PostMemTableCallback* post_memtable_callback;
    uint64_t log_used;  // log number that this batch was inserted into
    uint64_t log_ref;   // log number that memtable insert should reference
    WriteCallback* callback;
    bool made_waitable;          // records lazy construction of mutex and cv
    std::atomic<uint8_t> state;  // write under StateMutex() or pre-link
    WriteGroup* write_group;
    SequenceNumber sequence;  // the sequence number to use for the first key

```

How sequence number is set in PipelinedWrite
I can use seq_used to know what's the exact sequence number
for each key in write batch.
This way I don't need to update code insed PipelinedWriteImpl()
It's just a bit more cleaner.
```
Status DBImpl::PipelinedWriteImpl(const WriteOptions& write_options,
                                  WriteBatch* my_batch, WriteCallback* callback,
                                  uint64_t* log_used, uint64_t log_ref,
                                  bool disable_memtable, uint64_t* seq_used) {
      SequenceNumber next_sequence = current_sequence;
      for (auto* writer : wal_write_group) {
        assert(writer);
        if (writer->CheckCallback(this)) {
          if (writer->ShouldWriteToMemtable()) {
            writer->sequence = next_sequence;
            size_t count = WriteBatchInternal::Count(writer->batch);
            next_sequence += count;
 
```

```
  Status PutCFWithFeatures(uint32_t column_family_id, const Slice& key,
                                     const Slice& value, const KeyFeatures& features) override {
    const auto* kv_prot_info = NextProtectionInfo();
    Status ret_status;

      Status PutCFImplWithFeatures(uint32_t column_family_id, const Slice&key,
                              const Slice& value, ValueType value_type,
                               const ProtectionInfoKVOS64* kv_prot_info) {
// optimize for non-recovery mode
    if (UNLIKELY(write_after_commit_ && rebuilding_trx_ != nullptr)) {
      ret_status = 
          mem->AddWithFeatures(sequence_, value_type, key, value, kv_prot_info,
                               db_,
                   concurrent_memtable_writes_, get_post_process_info(mem),
                   hint_per_batch_ ? &GetHintMap()[mem] : nullptr);



```

Add key sequence store to unordered_map
```
Cumulative writes: 9996K writes, 9996K keys, 9996K commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 4.26 MB/s
```

Why do we still see garbage size not zero?
```
Blob file count: 242, total size: 4.2 GB, garbage size: 0.8 GB, space amp: 1.2
```
[Status: Done ]

[Todo]
Implement model training and data sampling process.
How does lrb do  data sampling and csr data preparation?
sample_timestamp is the same for all training_data->emplace_back()
In lookup() method. future_interval depends on cur_seq and sample_time
In forget() method, future_interval depends on memory_window * 2
In evict() metdho, future_interval depends on  cur_seq, sample_time and memory_window
```
lookup() {
    sample()
        if (!meta._sample_times.empty()) {
            //mature
            for (auto &sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                uint32_t future_distance = current_seq - sample_time;
                training_data->emplace_back(meta, sample_time, future_distance, meta._key);
                ++training_data_distribution[1];
            }
 
    train()
}

forget() {
    sample()
            uint32_t future_distance = memory_window * 2;
            for (auto &sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                training_data->emplace_back(meta, sample_time, future_distance, meta._key);
                ++training_data_distribution[0];
            }
            //batch_size ~>= batch_size
            if (training_data->labels.size() >= batch_size) {
                train();
                training_data->clear();
            }
 
    train()
}

evict() {

    auto epair = rank();
    uint64_t &key = epair.first;
    uint32_t &old_pos = epair.second;
    if (memory_window <= current_seq - meta._past_timestamp) {
        //must be the tail of lru
        if (!meta._sample_times.empty()) {
            //mature
            uint32_t future_distance = current_seq - meta._past_timestamp + memory_window;
            for (auto &sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                training_data->emplace_back(meta, sample_time, future_distance, meta._key);
                ++training_data_distribution[0];
            }
            //batch_size ~>= batch_size
            if (training_data->labels.size() >= batch_size) {
                train();
                training_data->clear();
            }
            meta._sample_times.clear();
            meta._sample_times.shrink_to_fit();
 

}
```
```
rank()
auto n_new_sample = sample_rate - idx_row;
    while (idx_row != sample_rate) {
        uint32_t pos = _distribution(_generator) % in_cache_metas.size();
        auto &meta = in_cache_metas[pos];
        if (key_set.find(meta._key) != key_set.end()) {
            continue;
        } else {
            key_set.insert(meta._key);
        }
#ifde
    if (meta._extra) {
            for (j = 0; j < meta._extra->_past_distance_idx && j < max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % max_n_past_distances;
                uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices[idx_feature] = j + 1;
                data[idx_feature++] = past_distance;
                if (this_past_distance < memory_window) {
                    ++n_within;
                }
//                } else
//                    break;
     

    for (uint8_t k = 0; k < n_edc_feature; ++k) {
            indices[idx_feature] = max_n_past_timestamps + n_extra_fields + 2 + k;
            uint32_t _distance_idx = min(uint32_t(current_seq - meta._past_timestamp) / edc_windows[k],
                                         max_hash_edc_idx);
            if (meta._extra)
                data[idx_feature++] = meta._extra->_edc[k] * hash_edc[_distance_idx];
            else
                data[idx_feature++] = hash_edc[_distance_idx];
        }

```
future_interval is used as label.
sample_time is used for each training_data->emplace_back()
I don't think I can use sample_time because write is different from read.
Still don't know how to prepare training data during run time.

In rank()
Only past_distance is used in distance data preparation.
```
        if (meta._extra) {
            for (j = 0; j < meta._extra->_past_distance_idx && j < max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % max_n_past_distances;
                uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];

```
But there is edc[k] * hash_edc[hash_edc_idx] in edc calculation.
I don't know why.
```
        for (uint8_t k = 0; k < n_edc_feature; ++k) {
            indices[idx_feature] = max_n_past_timestamps + n_extra_fields + 2 + k;
            uint32_t _distance_idx = min(uint32_t(current_seq - meta._past_timestamp) / edc_windows[k],
                                         max_hash_edc_idx);
            if (meta._extra)
                data[idx_feature++] = meta._extra->_edc[k] * hash_edc[_distance_idx];

```
edc feature is different for each rank  call because it uses current_seq - meta._past_timestamp 
I will use full data to train for now.
Later I will switch to sample method.
What kind of label should I give for one time write key? long? for sure.
What if I change task to regression?

For one time write key, need sample time - cur_seq as a edc to feed into dataset.
delta time will be really long.
How does lrb deal with key with only one time write?

Should I change task to regression?
It's hard classify key lifetime to short and long based on cdf right now.
Let's use 

How can I label a key feature?
Maybe I can just use _past_timestamp and cur_seq to label data?
In the future maybe I don't need to label at all. 
I can use unsupervised learning? No, maybe just learn from past writing keys. 
[Status: Done]

[Todo]
Regenerate seq timestamp features for analysis of lifetime in terms of sequence timestamp.
It takes really long time to finish one time features generation.
It takes 7.7 hrs to finish feature generation script for 20M line of data.
That is too slow.
```
python3 write_analysis.py  27794.36s user 158.92s system 100% cpu 7:45:17.02 total
```

```
num_zeros:  3981725
count of seq_latter:  10001651
avg of lifetime non zero:  1324264.0223846606
80th percentile of lifetime df_seq_latter:  1256369.0
65th percentile of lifetime df_seq_latter:  94293.5
avg of lifetime:  797065.5464000894
max of lifetime:  9982789.0
min of lifetime:  0.0
std of lifetime:  1634344.4297565364
```
[Status: Done]

[Todo]
Add training params at db::open()
[Status: Done]

[Todo]

Update creation timestamp in blob_file_meta to use sequence number 
Don't know if it's good idea to use seq number as timestamp.
One strategy: if seq of current key is not gced in this gc then 
we move this key to higher lifetime classification.
For now we leave keys classified as long lifetime and put it in blob file with
higer blob file number. But this doesn't change the fact that lower sequence 
keys are still being involved in gc soon.
How can I solve this problem?
Actually this is not a problem.As long as there is no sequence number increasing 
gc is not started. 
But we can't not start gc when the condition that cur_seq - min_seq > threshold.
We need to change condition to cur_seq - creation_seq > threshold .
```
if (!mutable_cf_options->disable_auto_compactions && !cfd->IsDropped()) {
  // NOTE: try to avoid unnecessary copy of MutableCFOptions if
  // compaction is not necessary. Need to make sure mutex is held
  // until we make a copy in the following code
  TEST_SYNC_POINT("DBImpl::BackgroundCompaction():BeforePickCompaction");
  c.reset(cfd->PickCompaction(*mutable_cf_options, mutable_db_options_,
                              log_buffer));

    Compaction* ColumnFamilyData::PickCompaction(
        const MutableCFOptions& mutable_options,
        const MutableDBOptions& mutable_db_options, LogBuffer* log_buffer) {
      auto* result = compaction_picker_->PickCompaction(
          GetName(), mutable_options, mutable_db_options, current_->storage_info(),
          log_buffer);
      if (result != nullptr) {
        result->SetInputVersion(current_);
      }
     
        Compaction* LevelCompactionBuilder::GetCompaction() {
            void VersionStorageInfo::ComputeCompactionScore(
                const ImmutableOptions& immutable_options,
                const MutableCFOptions& mutable_cf_options) {
             
              if(mutable_cf_options.enable_blob_garbage_collection && mutable_cf_options.blob_garbage_collection_age_cutoff > 0.0) {
                ComputeFilesMarkedForForcedBlobGCWithLifetime(mutable_cf_options.blob_garbage_collection_age_cutoff);
         
```
Need to pass VersionSet to VersionStorageInfo or ComputeFilesMarkedForForcedBlobGCWithLifetime()
Don't what should I do to add cur_seq to ComputeFilesMarkedForForcedBlobGCWithLifetime()
I think I can just pass versions_ to VersionStorageInfo
Add another constructor of VersionStorageInfo to


[Status: Done]

[Todo]
Add new lifetime to compaction_iterator.
For now I will just original lifetime label
because compaction traffic already check validity.
We can move keys classified as short lifetime to longer lifetime.
Or we can call model again during prepareoutput() function to decide   
whether how can we adjust lifetime of each key. 
For now I use original lifetime label. 
[Status: Done]

[Todo]
Update features featching in 
bool CompactionIterator::ExtractLargeValueIfNeededImpl() {
```
LIGHTGBM_C_EXPORT int LGBM_BoosterPredictForCSRSingleRowFast(FastConfigHandle fastConfig_handle,
                                                             const void* indptr,
                                                             const int indptr_type,
                                                             const int32_t* indices,
                                                             const void* data,
                                                             const int64_t nindptr,
                                                             const int64_t nelem,
                                                             int64_t* out_len,
                                                             double* out_result);


 * \brief Initialize and return a ``FastConfigHandle`` for use with ``LGBM_BoosterPredictForCSRSingleRowFast``.
 *
 * Release the ``FastConfig`` by passing its handle to ``LGBM_FastConfigFree`` when no longer needed.
 *
 * \param handle Booster handle
 * \param predict_type What should be predicted
 *   - ``C_API_PREDICT_NORMAL``: normal prediction, with transform (if needed);
 *   - ``C_API_PREDICT_RAW_SCORE``: raw score;
 *   - ``C_API_PREDICT_LEAF_INDEX``: leaf index;
 *   - ``C_API_PREDICT_CONTRIB``: feature contributions (SHAP values)
 * \param start_iteration Start index of the iteration to predict
 * \param num_iteration Number of iterations for prediction, <= 0 means no limit
 * \param data_type Type of ``data`` pointer, can be ``C_API_DTYPE_FLOAT32`` or ``C_API_DTYPE_FLOAT64``
 * \param num_col Number of columns
 * \param parameter Other parameters for prediction, e.g. early stopping for prediction
 * \param[out] out_fastConfig FastConfig object with which you can call ``LGBM_BoosterPredictForCSRSingleRowFast``
 * \return 0 when it succeeds, -1 when failure happens
 */
LIGHTGBM_C_EXPORT int LGBM_BoosterPredictForCSRSingleRowFastInit(BoosterHandle handle,
                                                                 const int predict_type,
                                                                 const int start_iteration,
                                                                 const int num_iteration,
                                                                 const int data_type,
                                                                 const int64_t num_col,
                                                                 const char* parameter,
                                                                 FastConfigHandle *out_fastConfig);

```


[Todo]
Update booster_config and booster_handle after each trainmodel()
Code example to call LGBM_BoosterPredictForCSRSingleRowFast
```
#include <lightgbm/c_api.h>
#include <stdlib.h>

int main() {
    // Assume we have a trained model and a FastConfigHandle
    FastConfigHandle fastConfig_handle;

    // Define a single row of data in CSR format
    int indptr_type = C_API_DTYPE_INT32;
    int64_t nindptr = 2;
    int64_t nelem = 3;

    int* indptr = (int*)malloc(nindptr * sizeof(int));
    indptr[0] = 0;
    indptr[1] = nelem;

    int32_t* indices = (int32_t*)malloc(nelem * sizeof(int32_t));
    indices[0] = 0;
    indices[1] = 2;
    indices[2] = 4;

    double* data = (double*)malloc(nelem * sizeof(double));
    data[0] = 1.0;
    data[1] = 3.0;
    data[2] = 5.0;

    // Output variables
    int64_t out_len;
    double* out_result = (double*)malloc(sizeof(double));

    // Call the function
    int result = LGBM_BoosterPredictForCSRSingleRowFast(fastConfig_handle, indptr, indptr_type, indices, data, nindptr, nelem, &out_len, out_result);

    // Check the result
    if (result == 0) {
        printf("Prediction successful, output length: %lld\n", out_len);
    } else {
        printf("Prediction failed, error code: %d\n", result);
    }

    // Free allocated memory
    free(indptr);
    free(indices);
    free(data);
    free(out_result);

    return 0;
}

```
[Status: Done]

[Todo]
Prepare data for training in 
bool CompactionIterator::ExtractLargeValueIfNeededImpl() {
I prepared data for training when seq % 1M == 0
[Status: Done]

[Todo]
Consider to use mutex when doing model free?
I can use shared_ptr<Model> to achieve model destruction.
[Status: Done]

Order of features:
1. cur_seq - past_seq
2. [past_distances]
3. value_size
3. n_within
4. [edc features]
Memory to reserve:
max_n_past_timestamps + n_edc_feature + 3 = 44
44

lightgbm functions need to be called
```
 int LGBM_BoosterPredictForCSRSingleRowFastInit(BoosterHandle handle,

int LGBM_BoosterPredictForCSRSingleRowFast(FastConfigHandle fastConfig_handle,
```
Finish model trianing and model continuous update. 
Need to check lifetime threshold later.
Run a db_bench experiment.

[Todo]
Fix model nullptr bug.
No model yet at the begining of the db_bench.
Delete this condition.
[Status: Done]

[Todo]
Fix nullptr of meta_extra bug when there is only one write of key. 
[Status: Done]

[Todo]
Fix indptr seg fault bug.
Don't know the root cause yet, need to check gdb msg.
Need to update num_features.
I think this is the root cause ?
```
[LightGBM] [Fatal] Check failed: (inner_data.first) < (num_col) at /home/zt/rocksdb_kv_sep_lightgbm_binary_model/third-party/lightgbm/src/c_api.cpp, line 1373 .
```
Update num_features in benchmark.sh from 21 to 44.
[Status: Done]

[Todo]
Fix compaction_ nullptr seg fault in compaction iterator.
The root cause is that compaction_ is nullptr in WriteLevel0Table()
How can we solve this problem?
Update constructor of compaction iterator.
Need to update version_set featching as well.
Add version_set to constructor of compaction_iterator.
Remove assert(false) when no meta_extra_ in key_meta.
[Status: Done]

[Todo]
Fix lightgbm predict calling seg fault bug.
check gdbt.cpp gdbt.h
```
template<typename T>
std::function<std::vector<std::pair<int, double>>(T idx)>
RowFunctionFromCSR(const void* indptr, int indptr_type, const int32_t* indices, const void* data, int data_type, int64_t , int64_t ) {
  if (data_type == C_API_DTYPE_FLOAT32) {
    if (indptr_type == C_API_DTYPE_INT32) {
     return RowFunctionFromCSR_helper<T, float, int32_t>(indptr, indices, data);
    } else if (indptr_type == C_API_DTYPE_INT64) {
     return RowFunctionFromCSR_helper<T, float, int64_t>(indptr, indices, data);
    }
  } else if (data_type == C_API_DTYPE_FLOAT64) {
    if (indptr_type == C_API_DTYPE_INT32) {
     return RowFunctionFromCSR_helper<T, double, int32_t>(indptr, indices, data);
    } else if (indptr_type == C_API_DTYPE_INT64) {
     return RowFunctionFromCSR_helper<T, double, int64_t>(indptr, indices, data);
    }
  }
  Log::Fatal("Unknown data type in RowFunctionFromCSR");
  return nullptr;
}
```

Change fast_config_handle.get() to *(fast_config_handle.get()) in
```
      int predict_res = LGBM_BoosterPredictForCSRSingleRowFast(*(fast_config_handle_.get()), 
                                                               indptr, C_API_DTYPE_INT32,
                                                               indices,
                                                               data,
                                                               2,
                                                               feature_idx+1,
                                                               &out_len,
                                                               out_result.data());
 
```

[Status: Done]

[Todo]
key_metas_ shows seg fault at key_metas_>at(key_str);
```
(gdb) p version_set_->LastSequence()
$4 = 1057724
(gdb) p ikey().sequence
$5 = 1046772
```
Any other write method that doesn't update key meta?
Add key existence checker to check key existence for each write 
but still get assert(false) in compaction_iterator. 
Is this because of lack of concurrent control of unordered_map?
I think this is the root cause. 
It may hapen the there is rehash during find of unordered_map.
Let's use thread-safe hash map.
I think this will solve the problem.
Will try shared_lock later. For now I will just use mutex to update unordered_map.
Will add shard mutex to increase parallelism. 

Now I add mutex to flush job and compaction iterator.
Finish db running after adding mutex when accessing unordered_map.
[Status: Done]


```
Uptime(secs): 2094.8 total, 1.0 interval
Cumulative writes: 10M writes, 10M keys, 10M commit groups, 1.0 writes per commit group, ingest: 7.88 GB, 3.85 MB/s
Cumulative WAL: 10M writes, 0 syncs, 10000000.00 writes per sync, written: 7.88 GB, 3.85 MB/s
Cumulative stall: 00:00:0.000 H:M:S, 0.0 percent
Interval writes: 0 writes, 0 keys, 0 commit groups, 0.0 writes per commit group, ingest: 0.00 MB, 0.00 MB/s
Interval WAL: 0 writes, 0 syncs, 0.00 writes per sync, written: 0.00 GB, 0.00 MB/s
Interval stall: 00:00:0.000 H:M:S, 0.0 percent

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     44.4    146.03            139.21        85    1.718       0      0       0.0       6.1
  L1      8/0   20.94 MB   0.8      3.6     0.3      0.7       0.9      0.2       0.0   1.2      7.2      7.1    514.08            513.58       108    4.760     27M  1563K       2.7       2.7
  L2     47/1   118.05 MB   0.6     11.3     0.1      1.0       1.1      0.1       0.0   1.1     10.8     10.7   1070.56           1069.36       361    2.966     34M  2033K      10.1      10.1
 Sum     55/1   138.99 MB   0.0     14.9     0.4      1.7       2.2      0.5       0.0   3.3      8.8     12.5   1730.67           1722.16       554    3.124     62M  3597K      12.8      18.9
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0     -0.0       0.0 68435187.0     13.6     13.5      4.83              4.83         1    4.828     88K    11K       0.1       0.1

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0     14.9     0.4      1.7       2.0      0.3       0.0   0.0      9.6      9.5   1584.64           1582.94       469    3.379     62M  3597K      12.8      12.8
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.3      0.3       0.0   0.0      0.0     44.4    146.03            139.21        85    1.718       0      0       0.0       6.1

Blob file count: 78, total size: 4.0 GB, garbage size: 0.9 GB, space amp: 1.3

Uptime(secs): 2094.8 total, 8.0 interval
Flush(GB): cumulative 6.325, interval 0.000
AddFile(GB): cumulative 0.000, interval 0.000
AddFile(Total Files): cumulative 0, interval 0
AddFile(L0 Files): cumulative 0, interval 0
AddFile(Keys): cumulative 0, interval 0
Cumulative compaction: 21.10 GB write, 10.31 MB/s write, 14.90 GB read, 7.28 MB/s read, 1730.7 seconds
Interval compaction: 0.06 GB write, 8.16 MB/s write, 0.06 GB read, 8.20 MB/s read, 4.8 seconds

ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               138MB   4GB     21.2    3.5     10.3    1737    1728    13      19      1984887333.0                                            2100    0.0     0       3.6     0.1     9.2    replay.t1.s1     2024-02-16T17:27:32     8.0.0           b86c01870b      0.5
```
[Status: Done]


[Todo]
Do not use any model when we don't have model at the begining.
```
Status DBImpl::FlushMemTableToOutputFile(

    flush_job.SetModelAndData(lightgbm_handle_, lightgbm_fastConfig_, &features_);
    s = flush_job.Run(&logs_with_prep_tracker_, &file_meta,
                      &switched_to_mempurge);
```
Need a better way to pass model to flush job then to compaction iterator.
I can just leave current model pass as it is.
Each FlushJob will get newest model from db_impl.
[Status: Done]



Current gc process still incurs space amplification because blob
file is only deleted after all linked ssts are deleted. 

[Todo]
Add invalid and valid keys ratio for each gc compaction job
[Status: Not started]

[Todo]
Pick smallest gc blob file number during ComputeFilesMarkedForForcedBlobGC()
```
class BlobFileMetaData {
 public:
  using LinkedSsts = std::unordered_set<uint64_t>;

```
LinkedSsts is unordered. We can sort sst file number before we return from
ComputeFilesMarkedForForcedBlobGC()
My solution is to store sst file nunmber of blob file in vector and 
sort the vector before appending sst file number into files_marked_for_forced_blob_gc_     
Finished. Left to running experiment and observe log.
[Status: Done]

[Todo]
Start new thread when training model.
Do not block frontend user op.
[Status: Not started]
I think I should start implement features collection module right now.

For now I think I can use model to do prediction each time we have a gc.
But what's the point of this if we store a map that stores all keys ?
Need to have a clever solution.

The whole point of this problem is that value with similar lifetime short be 
gathered together.

Should I move keys to shorter lifetime?
Three possible ways to get new lifetime for keys.
1. Stay in current lifetime classification bucket
2. Move to lower lifetime bucket
3. Move to higher lifetime bucket.
We can move keys between different lifetime bucket.

[Todo]
Set verbose_eval = -1 to suppress lightgbm training output
[Status: Not started]

[Todo]
Figure out why -inf gain in lightgbm training process. 
There is no lifetime:1 blob files now. Why is that ? 
I will check training code. And then I will update nelems in predict code 
in compaction iterator.
Update label threshold from 10M to 1M. 
Got three blobs labelled as 1 which is not what I expect..
For now I can lower threshold to 0.5M.
Two lifetime:1 blob after chaing threashold to 0.5M. Why is that ?

Update feature_idx+1 to feature_idx in
```
      int predict_res = LGBM_BoosterPredictForCSRSingleRowFast(*(fast_config_handle_.get()), 
                                                indptr, C_API_DTYPE_INT32,
                                                               indices,
                                                               data,
                                                               2,
                                                               feature_idx,
                                                               &out_len,
                                                               out_result.data());
 
```
during model calling in compaction iterator.

No change update change above. I will log ratio of long and short keys to
see if this is because of label issue.
No label issue.
```
 key label ratio 0: 357709, 1: 319766
```
Is this because of inadequate training iterations?
Need to add test set and see regression acc. 
What function should I call ?
Let's just print predict result for now.
Later I will call 
```
LIGHTGBM_C_EXPORT int LGBM_BoosterGetEval(BoosterHandle handle, int data_idx, int *out_len, double *out_results)
```

all predict res is the same
```
3out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03out_len: 1, result: 0.03o
```

Let's check predict res for training data.

Maybe use different model prediction method.
Still no difference of long lifetime blobs after changing model calling function.

Need to check if it's because of data issue.
How to check data issue?

Or it might be because of model passing issue?

Check num_features in compaction iterator.
```
(gdb) p data
$12 = {44055, 376412, 793, 1, 1.1368683772161603e-13, 4.76837158203125e-07, 0.0009765625,
  0.03125, 0.25000005960464478, 0.500244140625, 1.03125, 1.25, 1.5, 2,
  1.1588922146506942e-310, 4.6355705667425053e-310, 1.1588815830307155e-310,
  1.1588922146621565e-310, 1.1588922146522752e-310, 4.6355705666175067e-310,
  1.1588922146538562e-310, 1.1588922146621565e-310, 1.1588922146538562e-310,
  4.6355705665276361e-310, 1.1588815830307155e-310, 1.1588922146621565e-310,
  1.1588922146554372e-310, 4.635570566380207e-310, 1.1588922146585993e-310,
  1.1588922146700616e-310, 1.1588922146570183e-310, 4.6355705667425053e-310,
  1.1588922146578088e-310, 1.1588922146700616e-310, 1.1588922146585993e-310,
  1.1588922146795476e-310, 1.1588922146593898e-310, 4.6355705665247706e-310,
  1.1588922146601803e-310, 1.1588922146795476e-310, 1.1588922146609708e-310,
  4.6355705663786259e-310, 1.1588922146617613e-310, 1.1588922146795476e-310}
```

Check data and don't find any issue.

Will use std::vector instead of naive array.
Update array to std::vector to pass data to model prediction.
If this is not working then I need to check model passing.

No change after updating array to std::vector.
Checking model pointer passing issue.
Model pointer is consistent at the first iteration.
This is weird.
```
$1 = (BoosterHandle) 0x155500f98730
(gdb) p *(booster_handle_.get())
$4 = (std::__shared_ptr<void*, (__gnu_cxx::_Lock_policy)2>::element_type) 0x155500f98730
```

```
(BoosterHandle) 0x1554d15be040
 0x1554d15be040
```

What should I do now ?
Update params to be the same as training params.
If this does not work. Then it may be because of data 
issue.
seq shift a lot since first training.

NO change after updating params.

The only thing I can do right now is to write all training and inference
data to file to see if this is because of data issue.

Know what's wrong after writing training and inference data to file.
Current result is normal. Because all keys during flush will be within
the 0.5 M boundary of latest seq.
Need to think about better labelling method.
Maybe switch to regression task and change label.

lrb first add training sample and then do meta update.

```

bool LRBCache::lookup(const SimpleRequest &req) {
    for (auto &sample_time: meta._sample_times) {
        //don't use label within the first forget window because the data is not static
        uint32_t future_distance = current_seq - sample_time;
        training_data->emplace_back(meta, sample_time, future_distance, meta._key);
        ++training_data_distribution[1];
    }

    meta.update(current_seq);
```
What's the point of forget()?
It removes key which is out of
memory window from cache for every timestamp increase.


I don't know why -inf . But it seems that the model is working.
[Status: Done]


[Todo]
Fix false assertion 
```
assert(!level_file.second->being_compacted);
```
Action: check ComputeFilesMarkedForForcedBlobGC() and do synchronization 
if necessary.
Add sst_meta->being_compacted check
```
            if (sst_meta->being_compacted) {
              continue;
            }
```
[Status: Done]

[Todo]
Update labelling process.
We can do batch key_meta update during flush.

For keys with at least two write we can get a lifetime label for previous one.
For keys with only one write, how should we label it?
Too short, then w_amp is increased.
Too long , then w_amp is decreased, space_amp is increased.
Let's set it to 1M for now which is short in the context of binary classification
task.

Add sample to UpdateKeyMeta()
Do training data adding when sample time is not empty when there is 
new write of the key.
Then we need to do model prediction at write?
For key with no write before , could set delta = cur_seq - 0.
Don't need to update meta during write. I can update meta during 
compaction iterator.

If we can find key_meta during flush then there is definitely 
two writes for the key. Otherwise we can safely set its lifetime to 1M.

Update meta before prediction or after prediction?
For now I try updating meta before prediction.
Fow now I will just use future distance as prediction target. 
Remove cur_seq - past_seq as first data feature.
When should I addtrainingsample()?
For now I call addtrainingsample() in UpdateKeyMeta() each time after write.
KeyMeta has to have at least two writes for model to train.
We can maintain extra past_seq_2 in key_meta.

It's very slow doing random sample.
How can we increase sample spped to constant time.
Do addtrainingsample each time a key overwite arrives. 
No model training after 20 flush.

Maybe the model is not trained. Need to check model prediction 
after training.

Is this because of cur_seq?
Remove deleter from shared_ptr creation.

Remove cur_seq from training_data and compaction iterator.
I don't understand.

It's because of the cur_seq issue in edc feature generation.
Fix the issue temporarily.
[Status: Done]

Start sample after number of keys is over some threshold?



[Todo]
May need to adjust lifetime of blob during compaction. Because current compaction/gc
logic does not change lifetime label of blobs. 
[Status: Not started]

[Todo]
Need to mention that mlsm outperforms with gc and without gc
to show that our method help user get rid of parameter selection.
[Status: Not started]

Thinking about combining hashkv and lifetime blob gc together to
collect data and train model. 
Hashkv allows group same keys together which enables efficient data 
feature collection.
dumplsm lifetime blob files allow blobs to be grouped by lifetime.


Store partial key_metas in file.

Write keymeta to key in lsm-tree. Read keymeta during prediction and feature 
update.


[Todo]
Store key meta in key in lsm-tree.
We can pack all key meta in lsm-tree key.
How to pack?
For deltas , we have to use uint64_t.
Value size is stored in lsm-tree. uint32_t
nwithin constant. uint8_t
edc  uint32_t -> 10 * 4 = 40
past_distances -> 32 * 8 = 256
Max total bytes: 301 
Min total bytes: 0 or 53
Cool. Let's do it.
We need to do a get each time we flush.
Or we can delay prediction to compaction at l1.
Where is key parsed?
Key will first write too sst.
Let's check talbe builder file.
When we write to memtable. We should not do get and update.
This can hurt write performance.

table_builder.h
block_based_table_builder.h

Seems like need to do lots of change to key format and how key is read.
mem table inserter?
memtable.cc
```cpp
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  //  checksum     : char[moptions_.protection_bytes_per_key]
  uint32_t key_size = static_cast<uint32_t>(key.size());
  uint32_t val_size = static_cast<uint32_t>(value.size());
  uint32_t internal_key_size = key_size + 8;
  const uint32_t encoded_len = VarintLength(internal_key_size) +
                               internal_key_size + VarintLength(val_size) +
                               val_size + moptions_.protection_bytes_per_key;

```
Don't think I can do change to this part.
I think I can use varint for delta. and fix32 for float which can save lots
of memory and storage.. Great.
We can change internal key format again during builder.
```cpp
// Returns the user key portion of an internal key.
inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= kNumInternalBytes);
  return Slice(internal_key.data(), internal_key.size() - kNumInternalBytes);
}

```

I can update extractuserkey(). Or I can put key features after 
key content. This does not change how sequence number is fetched.
But we need to update ExtractUserkey to remove more bytes. 
Need to update:
ExtractUserkey()
ParseInternalKey()

I can make similar UpdateInternalKey() to update key meta during 
flush.
```cpp
inline void UpdateInternalKey(std::string* ikey, uint64_t seq, ValueType t) {
  size_t ikey_sz = ikey->size();
  assert(ikey_sz >= kNumInternalBytes);
  uint64_t newval = (seq << 8) | t;

  // Note: Since C++11, strings are guaranteed to be stored contiguously and
  // string::operator[]() is guaranteed not to change ikey.data().
  EncodeFixed64(&(*ikey)[ikey_sz - kNumInternalBytes], newval);
}


```
block_builder.cc

```cpp

void BlockBasedTableBuilder::Add(const Slice& key, const Slice& value) {
    r->data_block.AddWithLastKey(key, value, r->last_key);
        inline void BlockBuilder::AddWithLastKeyImpl(const Slice& key,
                                                     const Slice& value,
                                                     const Slice& last_key,
                                                     const Slice* const delta_value,
                                                     size_t buffer_size) {

            // Add "<shared><non_shared><value_size>" to buffer_
            PutVarint32Varint32Varint32(&buffer_, static_cast<uint32_t>(shared),
                                        static_cast<uint32_t>(non_shared),
                                        static_cast<uint32_t>(value.size()));
          }


          buffer_.append(key.data() + shared, non_shared);
```


block_based_table_reader.h
```cpp
class BlockBasedTable : public TableReader {
  InternalIterator* NewIterator(const ReadOptions&,
                                const SliceTransform* prefix_extractor,
                                Arena* arena, bool skip_filters,
                                TableReaderCaller caller,
                                size_t compaction_readahead_size = 0,
                                bool allow_unprepared_value = false) override;


```


block_based_table_iterator.h
```cpp
class BlockBasedTableIterator : public InternalIteratorBase<Slice> {
```
block.cc
```cpp
void DataBlockIter::NextImpl() {
  bool is_shared = false;
  ParseNextDataKey(&is_shared);
}
```
I don't think I need to change block iterator.
The only thing I need to change is remove key meta from key before 
calling iterator->seek(key)
When to remove ?
I want to be fast.
No key meta from compaction iterator during flush job.
Have key meta from compaction iter.
Need to make sure key meta does not affect order.

dbformat.h
```cpp
inline int InternalKeyComparator::Compare(const Slice& a,
                                          SequenceNumber a_global_seqno,
                                          const Slice& b,
                                          SequenceNumber b_global_seqno) const {
  int r = user_comparator_.Compare(ExtractUserKey(a), ExtractUserKey(b));
  if (r == 0) {
    uint64_t a_footer, b_footer;
    if (a_global_seqno == kDisableGlobalSequenceNumber) {
      a_footer = ExtractInternalKeyFooter(a);
    } else {
      a_footer = PackSequenceAndType(a_global_seqno, ExtractValueType(a));
    }
    if (b_global_seqno == kDisableGlobalSequenceNumber) {
      b_footer = ExtractInternalKeyFooter(b);
    } else {
      b_footer = PackSequenceAndType(b_global_seqno, ExtractValueType(b));
    }
    if (a_footer > b_footer) {
      r = -1;
    } else if (a_footer < b_footer) {
      r = +1;
    }
  }
  return r;
}


```
```cpp
inline Status ParseInternalKey(const Slice& internal_key,
                               ParsedInternalKey* result, bool log_err_key) {
  uint64_t num = DecodeFixed64(internal_key.data() + n - kNumInternalBytes);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);

```
I can add a key meta size at the end of internal key . Before sequence or after sequence.
Before or after ?
Let's try before. Because ParseInternalKey() now assumes sequence number at the end of internal key.
Pack num past_distances in 1 byte.
Pack len of keymeta in 4 byte.
We can store key len in  keymeta len.: 7 byte.
Or we can store key meta in value in lsm-tree.
I think this is possible and a good decision as well.

```cpp

bool CompactionIterator::ExtractLargeValueIfNeededImpl() {
Status BlobFileBuilder::Add(const Slice& key, const Slice& value,
                            std::string* blob_index) {
 
  BlobIndex::EncodeBlob(blob_index, blob_file_number, blob_offset, blob.size(),
                blob_compression_type_);


  value_ = blob_index_;
```
So now we only need to update how blob is fetched. which saves us lots of time.
hooray.
Not key meta for one time write keys. 
Should we write 
Don't think I need to write nwithin to file. I can 
just do calculation on the fly.
There will be lots of misses get if there are lots of  one time writes.
How to do encode?
coding.h
```cpp
extern void PutFixed32(std::string* dst, uint32_t value);
extern void PutVarint64(std::string* dst, uint64_t value);
```
I need to use internal iterator.
How to call it?
If there is no past distances, then we can still call model with only once past 
write information.
Or we can just put keys with only one time to some default lifetime class.
Let's just call model for now.

Need to copy new past_distances and edcs to value.  

[Status: Done]

[Todo]
Update key meta no matter model is available or not.
If key is not found then we need to set up initial edc and 
put it in value of lsm-tree.
Should I rearrange the order of features.?
Let's keep it the same.
[Status: Done]

[Todo]
Fix no finished flush and dead lock if it is.
No sst or blob is generated.
Found the root cause. NewInternalIterator acquire lock while Flush job 
doesn't realease lock.
```
Status FlushJob::WriteLevel0Table() {

    db_mutex_->Unlock();

      s = BuildTable(
```
How can I solve this problem?

Check if there is a way of creating iterator without holding the lock.
```
InternalIterator* DBImpl::NewInternalIterator(const ReadOptions& read_options,
                                              Arena* arena,
                                              SequenceNumber sequence,
                                              ColumnFamilyHandle* column_family,
                                              bool allow_unprepared_value) {
  ColumnFamilyData* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = static_cast_with_check<ColumnFamilyHandleImpl>(column_family);
    cfd = cfh->cfd();
  }

  mutex_.Lock();
  SuperVersion* super_version = cfd->GetSuperVersion()->Ref();
  mutex_.Unlock();
  return NewInternalIterator(read_options, cfd, super_version, arena, sequence,
                             allow_unprepared_value);
}
```
I create another version of NewInternalIterator() which does not 
include mem iter and imm iter.
Will call super_version->Ref() before flush job run.
Need to fix issue that internal iter is nullptr.
db internal iterator is nullptr when there is no sst in lsm-tree.
Added nullptr check in NewInternalIterator and compaction iter.
Still got deadlock when fix nullptr issue.
Found that I did not comment previous iter creation.
Got Background IO error Corruption: Error while decoding blob index: Corrupted blob offset
to fix.
```

Status BuildTable(
    builder->Add(key, value);
        Status FileMetaData::UpdateBoundaries(const Slice& key, const Slice& value,
                                          SequenceNumber seqno,
                                          ValueType value_type) {

```
Update slice size check from slice.size() == 1 to slice.size() >= 1
Fix PutVarint32 to PutFixed32 in compaction iter.
Got GetVarint32() failure of past_distance_count . Checking the root cause.
May need to add feature data to new value during gc happening at l1-ln.
Added feature data to new value.

Still got dead lock even flush succeeds for multiple files.
Destructor of internal iter reuiqres lock which causes the deadlock.
I can remove this mutex lock from destructor.
Decide to move contruction of internal iter to flush job
where mutex is unlocked.
Fix db_iter set name typo in BuildTable()
Got blob index value slice zero size assertion failed error. 
Don't know what's worong.
Is this beacause that rocksdb deleted value during compaction if it checks 
key is deleted? I don't think so.
I see, is this because internal iterator return blob value instead of value in lsm-tree?
I don't think so.
Let's check code.
internal iter does not return blob value. It return blob index value.
So what should I do now?
Let's check 
Add slice size check at the end of ExtractLargeValueIfNeeded()
Need to check that for every call of blob_index.DecodeFrom()
Found that value_ is cleared during compaction filter.
So I think this is value value_ size is zero during compaction iterator?
Need further check.
ikey_.type is updated to kTypeDeletion which should not make ExtractLargeValueIfNeeded
happen.
This is not the root cause.
version_edit.cc 
```

Status FileMetaData::UpdateBoundaries(const Slice& key, const Slice& value,
  if (value_type == kTypeBlobIndex) {
    BlobIndex blob_index;
    const Status s = blob_index.DecodeFrom(value);
 
```
value_ is updated during gc. 
Does this mean that key_meta_slice's memory address is invalidated?
Should I use PinnableSlice?
Don't know what's wrong. 
I yse string to store key meta during gc to avoid problem above..
I think I found the root cuase. 
Key type is updated to deletion during compaction
so no key meta is reserved.
Need to keep key meta even if key is updated to delete type.
Comment value_.clear() in compaction filter.
No model training during flush..
Need to fix this.
sigabrt during training .

Add deleter to make shared ptr in model creation to fix this memory issue..

Got free(): invalid next size (normal) error during destructor of std::vector<double>
Maybe there is multiple flush job concurrently and they call train model concurrently.
Need to add lock.

lrb use train data mutex and model mutex to enable concurrent model training 
and data collection. We can learn from this.
Add lock_guard to addtrainingsample for now.
Will do more optimization later if this works.
Fix this issue after adding lock_guard.
```
** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      3/0   20.18 MB   0.8      0.0     0.0      0.0       0.5      0.4       0.0   1.0      0.0      5.5   1208.09            820.08        86   14.048    359K    24K       0.0       6.1
  L1     10/1   33.92 MB   1.2      2.5     0.4      0.6       1.0      0.3       0.0   1.3      6.2      6.0    405.62            405.25        91    4.457     20M  1236K       1.4       1.4
  L2     70/3   193.11 MB   0.9      8.9     0.2      1.1       1.3      0.1       0.0   1.1      7.7      7.6   1190.77           1189.67       338    3.523     30M  2016K       7.5       7.5
 Sum     83/4   247.21 MB   0.0     11.4     0.7      1.8       2.7      0.9       0.0   2.7      4.2      6.5   2804.48           2415.00       515    5.446     51M  3277K       8.9      15.0
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0      0.0       0.0 125491993.0      8.5      8.4     14.29             14.28         1   14.291    239K    34K       0.1       0.1

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0     11.4     0.7      1.8       2.3      0.5       0.0   0.0      7.3      7.2   1600.79           1599.32       430    3.723     51M  3277K       8.9       8.9
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.5      0.5       0.0   0.0      0.0      5.5   1203.69            815.68        85   14.161       0      0       0.0       6.1


Completed replay (ID: ) in 1830 seconds
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               247MB   4GB     17.7    3.1     10.2    2804    2415    9       15      1778525303.0                                            1779    0.0     0       3.5     0.2     4.0    replay.t1.s1     2024-02-27T14:19:24     8.0.0           b86c01870b      0.6
Blob file count: 177, total size: 4.4 GB, garbage size: 1.0 GB, space amp: 1.3

```

No lifetime label:0 after updating default lifetime label to 1.
```
** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      1/0    6.75 MB   0.3      0.0     0.0      0.0       0.5      0.5       0.0   1.0      0.0      6.0   1114.68            779.41        85   13.114       0      0       0.0       6.1
  L1     10/1   29.98 MB   1.0      0.9     0.4      0.4       0.8      0.3       0.0   1.7      4.5      4.0    200.42            200.24        22    9.110     15M  1268K       0.0       0.0
  L2     74/6   203.40 MB   0.9      6.3     0.3      2.7       2.9      0.2       0.0   1.7      6.0      5.9   1063.22           1061.73       908    1.171     63M  2091K       3.3       3.3
 Sum     85/7   240.13 MB   0.0      7.1     0.7      3.2       4.1      0.9       0.0   2.1      3.1      5.8   2378.32           2041.38      1015    2.343     79M  3360K       3.3       9.3
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0      0.0       0.0 70180161.0      7.1      6.8      9.88              9.87         2    4.942    528K    56K       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      7.1     0.7      3.2       3.6      0.5       0.0   0.0      5.8      5.6   1263.64           1261.97       930    1.359     79M  3360K       3.3       3.3
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.5      0.5       0.0   0.0      0.0      6.0   1114.68            779.41        85   13.114       0      0       0.0       6.1


ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               240MB   4GB     13.4    2.5     7.7     2378    2041    3       9       1778525405.0                                            1779    0.0     0       3.1     0.2     5.2    replay.t1.s1     2024-02-27T15:09:09     8.0.0           b86c01870b      0.6
```
default 1 lifeimte :486 files which is a lot. 1020 jobs. 
default 0 lifetime :  260 files . 522 jobs.
std rocksdb:  109 files.  146 jobs.
std rocksdb only has 150 files.


Too many sst file linked to one blob file because of continuous compaction.
Let's ignore this problem for now.

[Status: Done]

[Todo]
Need to check if blob files are gced correctly.
I think it is.
ForcedBlobGC job starts later than that in deafult 0 lifetime impl.
[Status: Done]

[Todo]
Add train data mutex to protect train data from current update.
[Status: Done]

[Todo]
Most keys are only written once.
So we should put keys to long lifetime first?
Result is not bad.
But there are too many sst and blob files. 3 time more than default 0.
[Status: Done]

[Todo]
Add creation time and gc time for blob files to make 
sure that time limit works as expected.
It works
[Status: Done]

[Todo]
Compared flush job cost time between mlsm and standard rocksdb.
Need to reduce flush job time as much as possible. 
Standard rocksdb flush cost 1M microseconds.
```
Flush lasted 982270 microseconds
```
mlsm flush cost 4M microseconds
```
Flush lasted 3620559 microseconds,
```

xinq's running show that flush last 200K microseconds
```
JOB 96] Flush lasted 188156 microseconds,
```
20 times longer than standard's rocksdb flush time.

```
Flush lasted 56598584 microseconds,
```
300 times longer than xinqi's std rocksdb run.
This might be because of lock issue.
Add this to todo list to optmized performance.
```

Flush lasted 86745724 microseconds,
```
461 times longer than xinqq's run.
80 times longer that std rocksdb.
model training contributes to this time.

[Status: Ongoing]

[Todo]
Adjust log reporting time to 1min.
low priority.
[Status: Not started]



[Todo]
Set lifetime category based on distribution .
[Status: Not started]

[Todo]
Can we have a better way to reduce number of ssts?
It's not good to have fragmented ssts.
[Status: Abandoned]

[Todo]
Need to mention how we deal with cold start.
For now we select 4M lifetime category as lifetime bucket when
there is no model.
[Status: Not started]

[Todo]
Update two db internal iter call to one call.
Right now compaction filter do a call ,
PrepareOutput() does another call.
[Status: Abandanoned]

[Todo]
Don't get iter from mem when creating db internal iterator for flush job. 
Maybe we can get internal iterator from versions directly..
This is important for mlsm to work correctly. 
[Status: Done]

Found that hashkv implement vlog which is wisckey.
So now I can compare to both hashkv and wisckey without me writing 
code. This is great.
KV stores to compare:
1. rocksdb 
2. rocksdb with kv separation with gc
3. hashkv
4. wisckey
5. badgerdb?
6. tikv? (don't want to. It's the same as rocksdb)
7. TerakDB ( I can do that , but don't know if read and write performance is much better that mine)


[Todo]
Add training sample during flush job?
Still need to deal with sample imbalance issue.
Let's build training example during flush job for now. 
[Status: Not started]

[Todo]
Change model calling from traiditional calling to fast calling.
[Status: Not started]


[Todo]
Maintain write cache or insert key into read cache to 
get feature efficiently in case of write heavy workload.
Need to see write rate and gc status for further action.
Low priority for now.
[Status: Not started]

Problem:
Current gc process is not efficient because it does not 
do gc based on the lifetime of keys.
wisckey propose naive gc process. which incurs read/write overhead
and write amplifcation.
hashkv.
NovKV.


Challenge:
1. How to get feature from limited access information.
2. What kind of feature?
3. How to reduce model impact on performance of read& write
4. How to store feature appropriately?
5. How to adapt to different access pattern or do continuous model update?

[Todo]
Update cur_seq in edc feature generation?
Understand edc calculation after reading anohter paper 
: Popularity prediction of facebook videos for higher quality streming
which gives formula derivation of edc calculation.
Learn that hash_edc[distance_idx] is kernel function that 
provides influence calculation on future popularity given delta time.
The smaller the delta time is the bigger value kernel function return..
memory window control impact of delta over the long run .
So now I should update cur_seq - key_meta.past_seq to 
past_seq - past_seq_2.
Or I could use edc directly.
Work well.
Need to adjust training sample adding.
```
ops_sec mb_sec  lsm_sz  blob_sz c_wgb   w_amp   c_mbps  c_wsecs c_csecs b_rgb   b_wgb   usec_opp50      p99     p99.9   p99.99  pmax    uptime  stall%  Nstall  u_cpu   s_cpu   rss     test   date     version job_id  githash
0               146MB   4GB     16.4    2.9     8.5     2745    2740    8       14      1847944315.0                                            1975    0.0     0       4.3     0.3     9.0    replay.t1.s1     2024-02-21T20:57:17     8.0.0           b86c01870b      0.5
```
[Status: Done]

[Todo]
Call waitforcompaction to save more storage space.
[Status: Not started]

[Todo]
Adjust training sample adding.
[Status: Not started]

[Todo]
Reduce compaction time.
1. unlock while doing model training. Start a new thread for model training.
2. Use cache to read key meta
3. Do one time internal iter get for both compaction filter and key meta fetching.
4. Create internal iter which only contains internal iter from levels above this key.
[Status: Not started]

[Todo]
Add drop keys count during compaction ?
Need to see acc of predictor.

Drop keys are not very high.
```

2024/02/27-15:37:54.394000 1303665 (Original Log Time 2024/02/27-15:37:54.393311) [db/compaction/compaction_job.cc:867] [default] compacted to: files[2 7 72 0 0 0 0 0] max score 0.99, MB/sec: 7.3 rd, 7.3 wr, level 2, files in(0, 1) out(1 +1 blob) MB in(0.0, 3.2 +6.5 blob) out(3.2 +6.5 blob), read-write-amplify(3.0) write-amplify(1.5) OK, records in: 59448, records dropped: 917 output_compression: NoCompression
```
Add drop rate count for blob files?
std rocksdb drop rate: 3.1 / 29  = 0.1068965
mlsm default 1 lifetime online model with value in lsm-tree:  3.3 / 79 = 0.041772

[Status: Abandoned]

Titan do wisckey similar gc 
https://cn.pingcap.com/blog/titan-design-and-implementation/

[Todo]
Implment terakdb like gc process.
Add blob file dependency and update blob file number during compaction 
based on blob file dependency.
NB!
Code in terakdb that is related to gc.
```

void DBImpl::BackgroundCallGarbageCollection() {
    Status DBImpl::BackgroundGarbageCollection(bool* made_progress,
```
How is gc ratio calculated?
It doesn't matter now. It will be good if we know it and mention it in the paper.
I think the way gc ratio calculated is that each key drop in sst will increase 
garbage count in corresponding value file.

What's important now is to figure out how linked vsst works.
It's in version_builder.cc .


Terakdb check validity based on input version iterator.
I can use this one.

Why do we need dependence version in version builder??
What's the difference between inheritence item and dependence iterm?
Dependence iterm is old vsst. Inheritence item is new vsst.
inheritance in sst table prop cinludes newer files?

Readability of terakdb is not good.
```
typedef chash_map<uint64_t, FileMetaData*> DependenceMap;
```
terakdb use dependencemap in storage info to provide file depence.
But what we need during read and compaction is that we need inheritance
map to give which blob has value.
Don't want to read terakdb code now.
What I need to do is to implement our own gc process.
[Status: Onhold]

[Todo]
Figure out how blob_gc_ratio or score is calculated in terakdb. 
```
  auto push_candidate = [&](FileMetaData* f) {
    if (f->is_gc_permitted() && !f->being_compacted) {
      GarbageFileInfo gc_blob(f);
      if (gc_blob.estimate_size <= fragment_size ||
          gc_blob.score >= mutable_cf_options.blob_gc_ratio ||
          gc_blob.f->marked_for_compaction) {
        candidate_blob_vec.emplace_back(gc_blob);
      }
    }
  };
```

compaction_picker.cc
```
// Try to perform garbage collection from certain column family.
// Resulting as a pointer of compaction, nullptr as nothing to do.
// GC picker's principle:
// 1. pick the largest score blob, which must more than gc ratio
// 2. fragment should be take away by the way
// 3. it marked for compaction
Compaction* CompactionPicker::PickGarbageCollection(
    const std::string& /*cf_name*/, const MutableCFOptions& mutable_cf_options,
    VersionStorageInfo* vstorage, LogBuffer* /*log_buffer*/) {
  // Setting fragment_size as one eighth target_blob_file_size prevents
  // selecting massive files to single compaction which would pin down the
  // maximum deletable file number for a long time resulting possible storage
  // leakage.
  size_t target_blob_file_size = MaxBlobSize(
      mutable_cf_options, ioptions_.num_levels, ioptions_.compaction_style);

  size_t fragment_size = mutable_cf_options.blob_file_defragment_size;
  if (fragment_size == 0) {

```

version_set.cc
```
void VersionStorageInfo::ComputeCompactionScore(
    const ImmutableCFOptions& immutable_cf_options,
    const MutableCFOptions& mutable_cf_options) {
  for (int level = 0; level <= MaxInputLevel(); level++) {
    double score;
    if (level == 0) {

// Calculate total_garbage_ratio_ as criterion for NeedsGarbageCollection().
  uint64_t num_antiquation = 0;
  uint64_t num_entries = 0;
  bool marked = false;
  for (auto& f : LevelFiles(-1)) {
    if (!f->is_gc_permitted()) {
      continue;
    }
    // if a file being_compacted, gc_status must be kGarbageCollectionCandidate
    marked |= f->marked_for_compaction;
    num_antiquation += f->num_antiquation;
    num_entries += f->prop.num_entries;
  }
  blob_marked_for_compaction_ = marked;
  total_garbage_ratio_ = num_antiquation / std::max<double>(1, num_entries);

  is_pick_compaction_fail = false;
  ComputeFilesMarkedForCompaction();
  C
```
version_builder.cc
```
        if (finish) {
          uint64_t entry_depended = std::max<uint64_t>(1, item.entry_depended);
          entry_depended = std::min(item.f->prop.num_entries, entry_depended);
          uint64_t num_antiquation = item.f->prop.num_entries - entry_depended;
          switch (item.f->gc_status) {
            case FileMetaData::kGarbageCollectionForbidden:

```
What does entry_depended mean?
Does it mean that number of keys in sst file pointing to vsst?
[Status: Onhold]

[Todo]
Store blob offset map in hash map during gc.
Store this map items in wal to avoid data loss and map relation loss.
Rebuild this map after reboot.
Store smallest blob file number of all keys in sst file meta.
We can safely delete blob file map after no sst holds blob file number 
<= this blob file number.

Design change.
Store old blob file number + offset : new offset 
in new blob file number.
We need to store blob dependency file map to get blob file mapping.
This way we don't need to cost memory with more keys insertion.
How to build blob file depdendency map?
Store blob offset map or index blob in separate file because 
we need to maintain immediate mapping.
We can delete immediate blob file but we can nnot delete 
this immediate mapping file.
We can also sotre this mapping cache.
How to implement this?
Create a new file for x.blob_index when doing gc.
x.bob_index should have the same file number as newly generated blob.
Iterate through old blob file and keep blob file offset and len.
check validity.
len keeps the same as valid value. 
Write [old blob file numer, old offset] : [ new offset, len ] to new blob dinex 
file.
This blob index file isfinished when newly gc blob file finish building.  

Need to build dependence map.
blob:1 -> blob:2, blob:3 
blob:3 -> blob:7, blob:8

How to do get?
Forget about cache for now.
First search blob index block of all blob index files that inherit from blob:1
Only one blob index file includes offset of key in blob:1,
then we repeat this process until the newest blob index file is searched.
How does terakdb do get?
block_based_table_reader. cc
Get()
```
          if (!get_context->SaveValue(
                  parsed_key,
                  LazyBuffer(&static_state,
                             {reinterpret_cast<uint64_t>(&biter)},
                             biter.value(), rep_->file_number),
                  &matched)) {
            done = true;
            break;

```

```
LazyBuffer Version::TransToCombined(const Slice& user_key, uint64_t sequence,
                                    const LazyBuffer& value) const {
  auto s = value.fetch();
  if (!s.ok()) {
    return LazyBuffer(std::move(s));
  }
  uint64_t file_number = SeparateHelper::DecodeFileNumber(value.slice());
  auto& dependence_map = storage_info_.dependence_map();
  auto find = dependence_map.find(file_number);
  if (find == dependence_map.end()) {
    return LazyBuffer(Status::Corruption("Separate value dependence missing"));
  } else {
    return LazyBuffer(
        this,
        {reinterpret_cast<uint64_t>(user_key.data()), user_key.size(), sequence,
         reinterpret_cast<uint64_t>(&*find)},
        Slice::Invalid(), find->second->fd.GetNumber());
  }
}
```

db_impl.cc
```
if (s.ok() && lazy_val != nullptr) {
    lazy_val->pin(LazyBufferPinLevel::DB);
    s = lazy_val->fetch();
  }


```

Finally found it 
This function is responsible fo fetching value from blob.
```
Status Version::fetch_buffer(LazyBuffer* buffer) const {
  auto context = get_context(buffer);
  Slice user_key(reinterpret_cast<const char*>(context->data[0]),
                 context->data[1]);
  uint64_t sequence = context->data[2];
  auto pair = *reinterpret_cast<DependenceMap::value_type*>(context->data[3]);
  if (pair.second->fd.GetNumber() != pair.first) {
    RecordTick(db_statistics_, READ_BLOB_INVALID);
  } else {
    RecordTick(db_statistics_, READ_BLOB_VALID);
  }
  bool value_found = false;
  SequenceNumber context_seq;
  GetContext get_context(cfd_->internal_comparator().user_comparator(), nullptr,
                         cfd_->ioptions()->info_log, db_statistics_,
                         GetContext::kNotFound, user_key, buffer, &value_found,
                         nullptr, nullptr, nullptr, env_, &context_seq);
  IterKey iter_key;
  iter_key.SetInternalKey(user_key, sequence, kValueTypeForSeek);
  auto s = table_cache_->Get(
      ReadOptions(), *pair.second, storage_info_.dependence_map(),
      iter_key.GetInternalKey(), &get_context,
      mutable_cf_options_.prefix_extractor.get(), nullptr, true);
  if (!s.ok()) {
    return s;
  }
  if (context_seq != sequence || (get_context.State() != GetContext::kFound &&
                                  get_context.State() != GetContext::kMerge)) {

```

Call stack
```
(gdb) bt
#0  terarkdb::TableCache::Get (this=0x155554a99320, options=..., file_meta=...,
    dependence_map=..., k=..., get_context=0x1555517fb040, prefix_extractor=0x0,
    file_read_hist=0x0, skip_filters=true, level=-1, inheritance=0x0)
    at /mnt/nvme1n1/zt/terarkdb/db/table_cache.cc:443
#1  0x000055555580c87b in terarkdb::Version::fetch_buffer (this=0x155553038000,
    buffer=0x1555517fc140) at /mnt/nvme1n1/zt/terarkdb/db/version_set.cc:1299
#2  0x00005555555aa4a3 in terarkdb::LazyBuffer::fetch (this=0x1555517fc140)
    at /mnt/nvme1n1/zt/terarkdb/include/rocksdb/lazy_buffer.h:507
#3  0x00005555559adc4b in terarkdb::LazyBuffer::dump(terarkdb::LazyBuffer&) && (
    this=0x1555517fb570, _target=...) at /mnt/nvme1n1/zt/terarkdb/util/lazy_buffer.cc:678
#4  0x00005555559500c2 in terarkdb::GetContext::SaveValue (this=0x1555517fbbd0,
    parsed_key=..., value=..., matched=0x1555517fb48a)
    at /mnt/nvme1n1/zt/terarkdb/table/get_context.cc:221
#5  0x0000555555926263 in terarkdb::BlockBasedTable::Get (this=0x155553007050,
    read_options=..., key=..., get_context=0x1555517fbbd0, prefix_extractor=0x0,
    skip_filters=false) at /mnt/nvme1n1/zt/terarkdb/table/block_based_table_reader.cc:2522
#6  0x00005555557ff80b in terarkdb::TableCache::Get (this=0x155554a99320, options=...,
    file_meta=..., dependence_map=..., k=..., get_context=0x1555517fbbd0,
    prefix_extractor=0x0, file_read_hist=0x155554b0d000, skip_filters=false, level=0,
    inheritance=0x0) at /mnt/nvme1n1/zt/terarkdb/db/table_cache.cc:477
#7  0x000055555580d191 in terarkdb::Version::Get (this=0x155553038000, read_options=...,
    user_key=..., k=..., value=0x1555517fc140, status=0x1555517fc0a0,
    merge_context=0x1555517fbe00, max_covering_tombstone_seq=0x1555517fbdc0, value_found=0x0,
    key_exists=0x0, seq=0x0, callback=0x0) at /mnt/nvme1n1/zt/terarkdb/db/version_set.cc:1380
#8  0x000055555565c2ed in terarkdb::DBImpl::GetImpl (this=0x155554ad8000, read_options=...,
```

I think one blob file will only flow to another blob file. It will not flow into multiple blob file.
This way we can directly know which blob file to search for in future get request
by following link instead of a graph.

Decided to make single map relation between gc blob files which means
one blob file can only flow into another new blob file but not multiple new 
blob file during gc.
This reduce the cost to search multiple blob file during get.

Store blob file depenece map in version storge info.
Update this map during version_builder.
Get() will first find the newest blob file from this dependence map
and then do value offset hash map or tree map to get the offset 
of value in newest blob file. And finally we read blob value and return 
to user.

For now I will just store value offset hash map in memory.
Later I will store this map in separate file.

Checking if blob file has other parts besides key and value.
There is also footer in blob file but I think it's irrelavent .
Add blob file offset in BlobLogSequentialReader to get offset 
for gc.

Need to store offset map during gc in garbage_collection job.
First create unordered_map in db_impl_compaction_flush.cc and then
pass this map pointer to garbage_collection job ,
and then pass this map pointerto version builder.
When to delete this map.? Once there is no sst that potentialy 
dependes on this blob file.
Need to create blob writer during gc.
sst value store blob file numer and offset, does it store len?
It has blob file number, offset and len.
We can save some bytes by removing blob file number. 
Let's skip that for now.

blob file num gen.
```
  const uint64_t blob_file_number = file_number_generator_();

  assert(immutable_options_);
  assert(!immutable_options_->cf_paths.empty());
  std::string blob_file_path =
      BlobFileName(immutable_options_->cf_paths.front().path, blob_file_number);


```
Need to add blob file addition and new blob offset mapping to version edit.
Check how compaction does this and copy it.

Need to add blobfileaddtion blob_file_addition.
```

  SubcompactionState(Compaction* c, const std::optional<Slice> _start,
                     const std::optional<Slice> _end, uint32_t _sub_job_id)
      : compaction(c),
        start(_start),
        end(_end),
        sub_job_id(_sub_job_id),
        compaction_outputs_(c, /*is_penultimate_level=*/false),
        penultimate_level_outputs_(c, /*is_penultimate_level=*/true) {

```
compaction_outputs_ is created in subcompaction state.

```
CompactionJob::CompactionJob(
    int job_id, Compaction* compaction, const ImmutableDBOptions& db_options,
   std::string full_history_ts_low, std::string trim_ts,
    BlobFileCompletionCallback* blob_callback, int* bg_compaction_scheduled,
    int* bg_bottom_compaction_scheduled)
    : compact_(new CompactionState(compaction)),
      compaction_stats_(compaction->compaction_reason(), 1),

```
Don't know how to deal with linked sst and linked blob files.
Need to track min blob file sst depends on and update its min blob 
file during compaction. 

linked_blob_files are update during UpdateBoundaries.
Does this matter?
linked_ssts are update during ApplyFileAddition()

We can update linked_blob_files during compaction job.
And then we can delete blob file offset mapping 
during version builder.
Number of blob files that are smaller than min  blob file number
of all sst file depends on is the number of blob files that we can delete.

Let's not delete this map for now .
We can do this step later.

Need to update get process to make get successful.

Need to make sure blob files are in ascendent order 
to allow binary search.

Currently blob value offset is stored in blob index 
instead of  key offset.
[Status: Done]


[Todo]
size of key in blob offest map is 9
while it's 10 in sst file.
Need to figure out why and fix this.
Is this because of encode issue during gc job?
It might be that the key is obsolte and the value is dropped. 
It should not happen.

encode code in gc job.
```
          BlobIndex::EncodeBlob(&prev_blob_index_str, blob_file_num, blob_offset, record.value_size,
                               CompressionType::kNoCompression);

```
I see, it's because of key meta data that is appended to the end of value in lsm-tree.
Need to remove key meta data from value before doing search in blob offset map.
[Status: Done]

[Todo]
Issue: storage_info_.GetBlobFileMap().size() is 0 which should not happen.
Need to fix this.
Check version builder .
Can VersionBuilder::SaveTo() and GetBlob happens concurrently ?
I see there is blob:10 in blob_file_map when I start gdb.
I don't think get holds any lock.
There is no blob file map in another get.
Is this because new version builder does not write blob file map to new version?
I think I need to update copy constructor of version storage info
to copy blob file map and blob offset map to new version storage info object.
Need to do blob file map deletion later. For now I just leave it here.
[Status: Done]

[Todo]
Update value in lsm-tree to point to latest blob file during compaction.
[Status: Not started]

Why is there so many sst files?

[Todo]
Make sure compaction filter not work.
Cumulateive compaction is too slow.
```
 with_model_gc_1.0_0.8 ls | grep sst  | wc
    504     504    5544
Cumulative compaction: 7.94 GB write, 4.59 MB/s write, 1.61 GB read, 0.93 MB/s read, 1500.9 seconds
```
std rocksdb
```
➜  with_gc_1.0_0.8 ls | grep sst  | wc
     56      56     616
Cumulative compaction: 28.10 GB write, 16.20 MB/s write, 21.84 GB read, 12.59 MB/s read, 804.2 seconds


Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
  L0      2/0    6.58 MB   0.5      0.0     0.0      0.0       0.3      0.3       0.0   1.0      0.0     81.7     79.92             79.54        82    0.975       0      0       0.0       6.1
 Sum     56/4   158.07 MB   0.0     21.8     0.4      0.6       1.1      0.5       0.0   4.4     27.8     35.8    804.19            801.97       141    5.704     29M  3107K      20.8      27.0
```
I comment out GCFilter creation and use defeault compaction_filter..

No compaction rate change after comment out GCFilter creation.
```
Cumulative compaction: 7.95 GB write, 4.59 MB/s write, 1.62 GB read, 0.94 MB/s read, 1504.8 seconds

Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
  L0      2/0   14.08 MB   0.6      0.0     0.0      0.0       0.4      0.4       0.0   1.0      0.0      5.7   1173.98            799.04        81   14.494       0      0       0.0       6.1
 Sum     83/0   242.62 MB   0.0      4.8     0.7      0.9       1.9      1.0       0.0   1.2      3.3      5.4   1504.77           1129.46       192    7.837     30M  3301K       3.2       6.1
```
L0 flush cost too much time to do get and model calling..
Maybe we can delay this model calling to L1 or L2
Maybe use cache to get feature. 
Model calssification is not working either..
Need to figure out why.
[Status: Done]

[Todo]
L0 flush cost too much time to do get and model calling..
Maybe we can delay this model calling to L1 or L2
Maybe use cache to get feature. 
First I need to figure out why there is so many sst files.
Too mayny level_max_size compaction reason which is not normal.
```
{"time_micros": 1709618367736725, "job": 228, "event": "compaction_started", "compaction_reason": "LevelMaxLevelSize", "files_L1": [709], "files_L2": [556, 560, 561], "gc_blob_files": [], "score": 1.68405, "input_data_size": 11272296, "oldest_snapshot_seqno": -1}
```
92 compaction whose reason is level_max_size.

40 level_max_size compaction in std rocksdb.

How score is calculated?
How input_data_size is calculated?

I see , we add extra key meta to value part so there is more sst files.
key_size=${KEY_SIZE:-20}
key size is 20 so it's expected that sst file size is larger than that 
in std rocksdb.
[Status: Done]

[Todo]
Change to use version iterator instead of db internal interator 
to make sure validity check happens in gc process version.

[Status: Not started]

[Todo]
Model calssification is not working either..
Need to figure out why.
Too many zero in trianing data. This should not happen.
This is not right.
Should call vector.reserve()
```cpp
    std::vector<uint64_t> past_distances(max_n_past_timestamps);
```

No edcs and past distances in training data.
Still all keys are predicted as 1.
No edcs data in inference. Added it.
Need to make sure edcs store float number correctly.
Do reinterpret_cast when writing edcs float to file 
```cpp
        PutFixed32(&blob_index_, reinterpret_cast<uint32_t&>(edcs[i]));
```
edcs now have non zero value.
But there is still no past distances value.

All key are classified as 0 after model is available. This is not right.
Total blob size drop from 4.1 to 3.6 GB
w-amp increase from  1.8 to 2.0
w-blob increase from 9 to 11 GB

Again, there is not past distances training data.
n_within is normal so this means that past_distances is written to value in lsm-tree. 
Found the root cause..
I reserve memory space but I did not call emplace_back of past_distances.
But what's weird is that cpp does not report error when I access past_distances[i] where i > 0.
code that write past_distances to value in lsm-tree which does not use past_distances.size()
```cpp
      for(size_t i = 0; i < past_distances_count-1; i++) {
```
Hope this works and then I can move on to do pointer update 
during compaction .
This will take a while to finish.
Still got quite amount of work items to finish before 
I can run full benchmark.

invalid_key_ratio is very low 0.2-0.3 after introduce model prediction. 
```
2024/03/06-22:30:01.748468 1941364 (Original Log Time 2024/03/06-22:30:01.721583) EVENT_LOG_v1 {"time_micros": 1709735401721540, "job": 226, "valid_key_num": 93169, "invalid_key_num": 18037, "valid_value_size": 74320731, "invalid_value_size": 14477034, "invalid_key_ratio": 0.162194}
```
Werid thing, all key are classified as 0 after first model is available.
All keys are classified as  1 after second model is available. 
Why is that ?
There is some value classified as 1 because there is no past_distance_count that > 0 of the key.
```

2024/03/06-22:15:03.204416 1941371 [db/blob/blob_file_builder.cc:489] [default] [JOB 68] Generated blob file #221: 93633 total blobs, 80616244 total bytes, lifetime label: 1 creation timestamp: 4138270
2024/03/06-22:15:55.108930 1941358 [db/blob/blob_file_builder.cc:489] [default] [JOB 74] Generated blob file #231: 55791 total blobs, 47805101 total bytes, lifetime label: 1 creation timestamp: 4413341
2024/03/06-22:20:55.784199 1941370 [db/blob/blob_file_builder.cc:489] [default] [JOB 131] Generated blob file #410: 33978 total blobs, 29195327 total bytes, lifetime label: 0 creation timestamp: 6120309
2024/03/06-22:21:17.423697 1941373 [db/blob/blob_file_builder.cc:489] [default] [JOB 132] Generated blob file #414: 54454 total blobs, 47066687 total bytes, lifetime label: 0 creation timestamp: 6207481
2024/03/06-22:21:29.495561 1941364 [db/blob/blob_file_builder.cc:489] [default] [JOB 133] Generated blob file #415: 34366 total blobs, 29530451 total bytes, lifetime label: 0 creation timestamp: 6256389
2024/03/06-22:21:40.071969 1941359 [db/blob/blob_file_builder.cc:489] [default] [JOB 135] Generated blob file #420: 55433 total blobs, 47903083 total bytes, lifetime label: 0 creation timestamp: 6329184
2024/03/06-22:24:47.214161 1941369 EVENT_LOG_v1 {"time_micros": 1709735087214152, "job": 174, "event": "gc_started", "gc_files": [410, 414, 415, 420, 422, 221, 222, 225, 226, 229, 231]}

2024/03/06-22:25:02.896741 1941369 [db/blob/blob_file_builder.cc:489] [default] [JOB 174] Generated blob file #541: 130577 total blobs, 112093311 total bytes, lifetime label: 0 creation timestamp: 7428544
2024/03/06-22:25:27.042375 1941369 [db/blob/blob_file_builder.cc:489] [default] [JOB 174] Generated blob file #564: 145649 total blobs, 124775488 total bytes, lifetime label: 1 creation timestamp: 7428544
2024/03/06-22:25:44.780826 1941369 [db/blob/blob_file_builder.cc:489] [default] [JOB 174] Generated blob file #583: 132445 total blobs, 113445916 total bytes, lifetime label: 1 creation timestamp: 7428544
2024/03/06-22:25:44.781440 1941369 [db/blob/blob_file_builder.cc:489] [default] [JOB 174] Generated blob file #558: 28755 total blobs, 24674080 total bytes, lifetime label: 0 creation timestamp: 7428544

```

```
 [JOB 138] Generated blob file #426: 55479 total blobs, 47938970 total bytes, lifetime label: 0 creation timestamp: 6450887
 [JOB 138] Generated blob file #427: 38167 total blobs, 32684780 total bytes, lifetime label: 1 creation timestamp: 6450887
```

```

2024/03/06-22:17:24.186355 1941373 [db/training/training_data.cc:307] key label ratio 0: 311489, 1: 188511 res_short_count: 297848, res_long_count: 202152
2024/03/06-22:24:23.522786 1941371 [db/training/training_data.cc:307] key label ratio 0: 195067, 1: 304933 res_short_count: 160066, res_long_count: 339934
2024/03/06-22:29:52.088006 1941372 [db/training/training_data.cc:307] key label ratio 0: 151436, 1: 348564 res_short_count: 118284, res_long_count: 381716
```
Seems model is biased towards majority of labeled keys.
But it should not happen that all keys are classified as 0 or 1.

Did I put blob files to their original lifetime bucket during gc ? I should put it 
to longer lifetime bucket because it proves that model classification is worng..
But for now I think I need to change model to do regression task.

This code is problematic.
Should decide past_distances_count before write it to value.
```cpp
    PutVarint32(&blob_index_, past_distances_count);
    if(past_distances_count > 0) {
      past_distances_count = std::min(past_distances_count, static_cast<uint32_t>(max_n_past_timestamps));
      PutVarint64(&blob_index_, distance ); 
      // write past_distances to value
      for(size_t i = 0; i < past_distances_count-1; i++) {
        PutVarint64(&blob_index_, past_distances[i]);
      }
 
```

All inference res of data is the same.
Need to check data issue.
```
789.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000122 1.015625 1.125000 1.500000 0.767660
792.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000488 0.767660
795.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000002 1.001953 0.767660
791.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000061 1.007812 0.767660
788.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000122 1.015625 1.125000 0.767660
791.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000061 0.767660
1591272.000000 1323121.000000 1433107.000000 814.000000 3.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000244 1.016129 1.164062 1.000000 1.000000 1.000000 1.000000 1.000001 1.000977 1.031250 1.250061 1.508064 2.164062 0.767660
809.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000122 1.015625 1.125000 1.500000 0.767660 
123487.000000 1135455.000000 3106464.000000 715994.000000 1919905.000000 168250.000000 821.000000 4.000000 1.000000 1.000000 1.000000 1.000031 1.007812 1.125000 1.500004 2.003906 2.062538 2.262695 1.000000 1.000000 1.000000 1.000000 1.000492 1.035156 1.375001 2.001953 3.062538 3.262695 0.767660
408329.000000 813.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000244 1.015625 1.125000 1.500000 2.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000732 1.062500 0.767660
799.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000977 0.767660
```
Is this because of edcs data issue?

Found another issue. 
I should not put edcs when I first read edcs from value.
```cpp
      if(past_distances_count> 0) {
        for(i=0; i < n_edc_feature; i++) {
          float edc;
          ok = GetFixed32(&prev_value, reinterpret_cast<uint32_t*>(&edc));
          edcs.emplace_back(edc);
          // indices.emplace_back(max_n_past_timestamps+2+i);
          // data.emplace_back(edcs[i]);
          assert(ok);
        }
      }

```
Prediction value is not working after comment code above. 
Need to do more check.
Is this because of db_itnernal_iter issue?
I should not create db_internal iter that refers memtable and immutable memtable..  
Add       assert(distance > 0); to make sure db internal iter 
does not retrieve keys that are in memtable.

There is no data in edcs when past_distances_count == 0.
So I call emplace_back(new_edc) to add new_edc to edcs.
But I don't think this step fix anything.
```cpp
      } else {
        for(size_t k=0; k < n_edc_feature; k++) {
          uint32_t _distance_idx = std::min(uint32_t(distance / edc_windows[k]), max_hash_edc_idx);
          float new_edc = hash_edc[_distance_idx]  + 1;
          edcs.emplace_back(new_edc);
          indices.emplace_back(max_n_past_timestamps+2+k);
          data.emplace_back(new_edc);
        }
      }


```

Got a assertion false 
```
db_bench: /home/zt/rocksdb_kv_sep_lightgbm_binary_model/db/compaction/compaction_iterator.cc:1427: bool rocksdb::CompactionIterator::ExtractLargeValueIfNeededImpl(): Assertion `data.size() < num_features_' failed.
```
Should change to <=.

This should not happen..
There should be at least one past_distance in infer data.
```
810.000000 0.000000 1.000000 1.000000 1.000000 1.000488 1.031250 1.250000 1.500000 2.000000 2.000000 2.000000 0.452911
810.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000000 1.000015 1.003906 0.452911
803.000000 0.000000 1.000000 1.000000 1.000000 1.000000 1.000122 1.015625 1.125000 1.500000 2.000000 2.000000 0.452911
```

Are fast_config handle updated? I think so

Add additional condition to model calling 
```cpp
      if(fast_config_handle_ && past_distances_count > 0) {
```

I have already implemented internal iterator starting from level 0
Still not change after doing code change above.
Let's remove edcs from feature..

Add indptr[1] = data.size()
Dont' know if this is the root cause.

Change model training trigger threshold from  500k to 100k toget fastter validation. 
Finally  prediciton value is normal after adding indptr[1]
Fuckx.

Invalid key ratio increases a lot after model is working.
But still got low invalid_key_ratio in some gc job.
```
2024/03/07-16:35:49.327204 885924 (Original Log Time 2024/03/07-16:35:49.324675) EVENT_LOG_v1 {"time_micros": 1709800549324647, "job": 50, "valid_key_num": 4036, "invalid_key_num": 9596, "valid_value_size": 3256271, "invalid_value_size": 7925179, "invalid_key_ratio": 0.703932}

2024/03/07-16:43:16.790429 885923 (Original Log Time 2024/03/07-16:43:16.779019) EVENT_LOG_v1 {"time_micros": 1709800996778985, "job": 143, "valid_key_num": 1158, "invalid_key_num": 384, "valid_value_size": 928509, "invalid_value_size": 311545, "invalid_key_ratio": 0.249027}
```

Too many small blobs. Why is that ?
There are some blob_file_deletion.
Not all blob are deleted.


[Status: Done]


[Todo]
Fix read blob assertion false issue.
It's because I did not call edit->AddBlobFileMap()
[Status: Done]

[Todo]
Change GetBlob to use new function with blob_index_slice
```
Status BlobFetcher::FetchBlob(const Slice& user_key,
                              const BlobIndex& blob_index,
                              FilePrefetchBuffer* prefetch_buffer,
                              PinnableSlice* blob_value,
                              uint64_t* bytes_read) const {
  assert(version_);


```
This code is called in invoke_filter and compaction iteration when 
doing blob_index gc. 
Update this function to encode blob_index into string and do the call.

[Status: Done]

[Todo]
Always return from ExtractLargeValueIfNeeded();    
No gc during compaction iterator.
[Status: Done]

[Todo]
Make sure blob file picking is correct.
Currently copmuteblobmarkforcompaction is called in AppendVersion .
Should we call it in other places?
```c++

void VersionSet::AppendVersion(ColumnFamilyData* column_family_data,
  v->storage_info()->ComputeBlobsMarkedForForcedGC(1.0);
```
[Status: Done]


[Todo]
Add blob file mapping.
Create blob file mapping during gc.
Need to create another function in version builder.  

[Status: Done]

[Todo]
Change blob file map and blob offset map from pointer to value.
There is a bug when doing ApplyBlobOffsetMap;
[Status: Done]

[Todo]
Make sure obsolete blob files are not inserted to new version
during version builder..
Need to check LOG and make sure gc blob files ared deleted.
This has been done in code I written. 
blob files in deleted_blob_files are not inserted to new version.
[Status: Done]


[Todo]
Need to mention that we create indirect blob offset map in our paper 
to solve one:one value offset binding between sst and blob file.
[Status: Not started]




What if one sst points to too many blob files?
Any harm?
What if many sst files point to one blob file?

[Todo]
Delay model prediction till compaction in level 0?
[Status: Not started]

[Todo]
May need to mention one disadvantage of our method is that 
it do more compaction compared to std rocksdb but with less write size.
[Status: Abandoned]

[Todo]
Remove key data collection to speed up write speed.
[Status: Not started]

[Todo]
Add write andread stats to compaction stats of garbage collection job.
db impl must maintain internal compaction stats.
I can run bench mark and set breakpoint to 
get call stack.
```cpp

Status CompactionJob::Install(const MutableCFOptions& mutable_cf_options) {
    int output_level = compact_->compaction->output_level();
    cfd->internal_stats()->AddCompactionStats(output_level, thread_pri_,
                                                compaction_stats_);


    void CompactionJob::UpdateCompactionJobStats(
        const InternalStats::CompactionStats& stats) const {

          compaction_job_stats_->elapsed_micros = stats.micros;
          compaction_job_stats_->total_output_bytes = stats.bytes_written;
          compaction_job_stats_->total_output_bytes_blob = stats.bytes_written_blob;  
```

```cpp

Status CompactionJob::Run() {
  ProcessKeyValueCompaction(&compact_->sub_compact_states[0]);
  compaction_stats_.SetMicros(db_options_.clock->NowMicros() - start_micros);

  compact_->AggregateCompactionStats(compaction_stats_, *compaction_job_stats_);

    // read stats.
    // add num_input_records
    // and bytes_read_blob
    // and num_droppped_records
  UpdateCompactionStats();
        void CompactionJob::UpdateCompactionInputStatsHelper(int* num_files,
                                                             uint64_t* bytes_read,
                                                             int input_level) {
          const Compaction* compaction = compact_->compaction;
          auto num_input_files = compaction->num_input_files(input_level);
          *num_files += static_cast<int>(num_input_files);
          for (size_t i = 0; i < num_input_files; ++i) {
            const auto* file_meta = compaction->input(input_level, i);
            *bytes_read += file_meta->fd.GetFileSize();
            compaction_stats_.stats.num_input_records +=
                static_cast<uint64_t>(file_meta->num_entries);
              }
          compaction_stats_.stats.bytes_read_blob =
              compaction_job_stats_->total_blob_bytes_read;
          compaction_stats_.stats.num_dropped_records =
              compaction_stats_.DroppedRecords();

            uint64_t DroppedRecords() {
              uint64_t output_records = stats.num_output_records;
              if (has_penultimate_level_output) {
                output_records += penultimate_level_stats.num_output_records;
              }
              if (stats.num_input_records > output_records) {
                return stats.num_input_records - output_records;
              }
              return 0;
            }


```
When CompactionStats is updated?
I can update CompactionStats during 
Actually I have already do gc write stats update in GarbageCollectionJob::Install()
```cpp
Status GarbageCollectionJob::Install(const MutableCFOptions& mutable_cf_options) {
  cfd->internal_stats()->AddGCStats(thread_pri_, gc_stats_);
```
Did I update read stats?

I add bytes_read_blob to sum level 
```cpp
void InternalStats::DumpCFMapStats(
    const VersionStorageInfo* vstorage,
    std::map<int, std::map<LevelStatType, double>>* levels_stats,
    CompactionStats* compaction_stats_sum) {
 
      compaction_stats_sum->bytes_read_blob += gc_stats_.bytes_read_blob;
```

```cpp
  void AddGCStats(Env::Priority thread_pri,
                  const CompactionStatsFull& gc_stats) {
    gc_stats_.Add(gc_stats.stats);
    comp_stats_by_pri_[thread_pri].Add(gc_stats.stats);

  }
      void Add(const CompactionStats& c) {
      this->micros += c.micros;
      this->cpu_micros += c.cpu_micros;
      this->bytes_read_non_output_levels += c.bytes_read_non_output_levels;
      this->bytes_read_output_level += c.bytes_read_output_level;
      this->bytes_read_blob += c.bytes_read_blob;
      this->bytes_written += c.bytes_written;
      this->bytes_written_blob += c.bytes_written_blob;
      this->bytes_moved += c.bytes_moved;
      this->num_input_files_in_non_output_levels +=
          c.num_input_files_in_non_output_levels;
      this->num_input_files_in_output_level +=
          c.num_input_files_in_output_level;
      this->num_output_files += c.num_output_files;
      this->num_output_files_blob += c.num_output_files_blob;
      this->num_input_records += c.num_input_records;
      this->num_dropped_records += c.num_dropped_records;
      this->num_output_records += c.num_output_records;
      this->count += c.count;
      int num_of_reasons = static_cast<int>(CompactionReason::kNumOfReasons);
      for (int i = 0; i < num_of_reasons; i++) {
        counts[i] += c.counts[i];
      }
    }
```
How micros and cpu_micros 
How does compaction iterator update bytes_written_blob ?
```cpp
  if(enable_blob_file_builder) {
    sub_compact->Current().UpdateBlobStats();
  }
  void UpdateBlobStats() {
    assert(!is_penultimate_level_);
    stats_.num_output_files_blob = blob_file_additions_.size();
    for (const auto& blob : blob_file_additions_) {
      stats_.bytes_written_blob += blob.GetTotalBlobBytes();
    }
  }

```
Don't think compaction_job_stats is useful.
Should I add separte garbage collection bytes? 
I need to add bytes_written and bytes_written_blob to compaction stats for now
to get true write amplifcation.
Do I need to call this ?
```cpp
db/compaction/compaction_job.cc|1829 col 14| RecordTick(stats_, COMPACT_WRITE_BYTES, IOSTATS(bytes_written));
```
I will not  call this for now.

call 
```cpp
void InternalStats::DumpCFMapStats(
    const VersionStorageInfo* vstorage,
    std::map<int, std::map<LevelStatType, double>>* levels_stats,
    CompactionStats* compaction_stats_sum) {
 
  compaction_stats_sum->Add(gc_stats_);
```
to add garbage collection job stats to internal stats.
I think now w-amp is correct.
w-amp does not change after adding bytes_written_blob to internal stats.
Why is that ?
```

** Compaction Stats [default] **
Level    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  L0      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.4      0.4       0.0   1.0      0.0      5.8   1164.33            796.51        81   14.374       0      0       0.0       6.1
  L1     12/1   39.10 MB   1.4      0.8     0.4      0.4       0.7      0.3       0.0   1.6      5.2      4.5    167.05            166.79        20    8.352     15M  1248K       0.0       0.0
  L2     69/3   195.28 MB   0.9      0.8     0.3      0.5       0.7      0.2       0.0   2.5      4.8      4.2    166.52            166.27        86    1.936     15M  1986K       0.0       0.0
  L3      4/0   10.78 MB   0.0      0.0     0.0      0.0       0.0      0.0       0.0   0.0      0.0      0.0      0.00              0.00         0    0.000       0      0       0.0       0.0
 Sum     85/4   245.16 MB   0.0      6.8     0.7      0.9       1.9      1.0       0.0   1.7      3.5      5.7   1987.85           1617.50       187   10.630     37M  5715K       5.2       9.3
 Int      0/0    0.00 KB   0.0      0.1     0.0      0.0       0.0      0.0       0.0 51494568.0      6.3      5.5      8.98              8.97         2    4.490    817K    77K       0.0       0.0

** Compaction Stats [default] **
Priority    Files   Size     Score Read(GB)  Rn(GB) Rnp1(GB) Write(GB) Wnew(GB) Moved(GB) W-Amp Rd(MB/s) Wr(MB/s) Comp(sec) CompMergeCPU(sec) Comp(cnt) Avg(sec) KeyIn KeyDrop Rblob(GB) Wblob(GB)
---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Low      0/0    0.00 KB   0.0      6.8     0.7      0.9       1.4      0.5       0.0   0.0      8.5      5.7    823.52            820.99       106    7.769     37M  5715K       5.2       3.2
High      0/0    0.00 KB   0.0      0.0     0.0      0.0       0.4      0.4       0.0   0.0      0.0      5.8   1164.33            796.51        81   14.374       0      0       0.0       6.1

Blob file count: 51, total size: 4.1 GB, garbage size: 0.0 GB, space amp: 1.0

Uptime(secs): 1773.8 total, 8.0 interval
Flush(GB): cumulative 6.543, interval 0.000
Cumulative writes: 9986K writes, 9986K keys, 9986K commit groups, 1.0 writes per commit group, ingest: 7.87 GB, 4.54 MB/s
```
Actually w-amp does change after I checked w-amp before.
w-amp before: 1.2
w-amp now: 1.7
wblob before: 6.1GB
wblob now: 9.3GB
drop rate before: 0.11
drop rate now:0.154
[Status: Done]

[Todo]
How cumulated compaction and writes is counted?
[Status: Not started]

[Todo]
Write blob offset map to file.
Low priority for now.
[Status: Not started]

[Todo]
figure out why blob files are not deleted and fix it..
```cpp
  Status ApplyFileDeletion(int level, uint64_t file_number) {
    const FileMetaData* const meta =
        base_vstorage_->GetFileMetaDataByNumber(file_number);
    for(auto blob_file_num: meta->linked_blob_files) {
      if(blob_file_num != kInvalidBlobFileNumber) {
        MutableBlobFileMetaData* const mutable_meta =
            GetOrCreateMutableBlobFileMetaData(blob_file_num);
        if (mutable_meta) {
          mutable_meta->UnlinkSst(file_number);
        }

      }

    const uint64_t blob_file_number=
        GetOldestBlobFileNumberForTableFile(level, file_number);
      if(blob_file_number != kInvalidBlobFileNumber) {
        if(meta->linked_blob_files.find(blob_file_number) == meta->linked_blob_files.end()) {
          assert(false);
        }

      }
```
Should not create mutable blob file meta if it's outdated.
blob_file_meta files are still in blob_files_.
Need to remove obsolte blob files from blob_files_
[Status: Ongoing]

[Todo]
Delete obsolete map items.
Need to do this during compaction.
What kind of data should we track?
Just delete blob map item by checking min_oldes blob.
Those blob file whose number is below min_oldest_blob are deleted.
Where is min_oldest_blob_file_numer updaet?
It's in UpdateBoundaries();
Need to do blob file map addition and blob offset map addition 
in Apply();
And then we do deletion at SaveTo() ?
Finally understand How SaveSSTFilesTo() works.
There are two vstorage. The first one is base_vstorage
and the second one is new vstorage.
So first get sst files from base vstorage 
and then copy that files to new vstorage.
I can do the same to blob files.
Need to work fast.
Need to get min oldest blob file from all sst's linked blob
files.

Get blob index decode error during run.
```
2024/03/08-12:10:58.676739 4088090 [ERROR] [db/db_impl/db_impl_compaction_flush.cc:3255] Waiting after background compaction error: Corruption: Error while decoding blob index: Unknown blob index type: -80, Accumulated background error counts: 1
```
I see, I didn't allocate heap memory for offset. map. 
But there is no garbage collection job yet.
Is this because of update boundaries? I think so.
I have already  allocate memory for offset map.
I think this is because of blob index update issue..
Need to do debug and find out hte root case and then fix it..

Maybe this is problematic.
Need to make sure blob type is correct.

const Slice Version::GetLatestBlobIndex(const uint64_t orig_blob_file_number,
returns a reference to a string that is not valid after the next call to GetLatestBlobIndex.
because that string allocated inside the function.
```
  if(orig_blob_without_key_meta.compare(latest_blob_index_str) == 0) {
```

Got another seg fault during compaction.
Want to print blob index file nunmber but failed to do that.
blob:129 offset is newly generated and it should not be deleted.
```
2024/03/08-15:42:03.378221 476979 [db/blob/blob_file_builder.cc:489] [default] [JOB 37] Generated blob file #129: 2869 total blobs, 2491621 total bytes, lifetime label: 0 creation timestamp: 2963778
2024/03/08-15:42:03.378742 476979 [db/version_builder.cc:751] blob offset map deletion: 129
```

gpt4 helps me find the bug.
I should emplace created shared_ptr instead to original raw pointer.

Need to log how many blob index is updated during compaction iterator.
I need to do this collborately with blob file meta deletion from blob_files_
Achieve this goal by set oldest blob file number.

Got assertino false error when doing compaction.
No offset item in new offset map.
I don't know what's wrong.
It might happen that the key in sst file is discarded during gc.
So it's normal that value offset is not found during compaction job.
[Status: Done]

[Todo]
Figure out how total blob size is calculated and fix it if there is issue. 
It count blob size by iterating all blob files of version storage info..
I think current blob size counting is correct.
But it's weird that previous version has similar blob size as this version.
[Status: Done]

[Todo]
Need to remove blob_file_map and blob_offset_map copy from copy constructor.
[Status: Done]

[Todo]
Need to update unodered_map<>* to shared_ptr<unodered_map>   
I think I need to create shared_ptr in version builder code. 
[Status: Done]

How blob->linkssts() works?;

[Todo]
Get read kops 
[Status: Not started]

[Todo]
Make sure obosolete blob files ared deleted after gc.
Currently not all blob files are deleted.
```bash
(base) ➜  with_model_gc_1.0_0.8 du -ch *.blob | grep total
8.3G    total
```

```
2024/03/08-19:30:47.999480 1095411 EVENT_LOG_v1 {"time_micros": 1709897447999475, "job": 142, "event": "blob_file_deletion", "file_number": 127}
```

```
"blob_file_head": 587, "blob_file_tail": 982, "blob_files": [587, 591, 595, 596, 599, 602, 603, 617, 627, 634, 651, 654, 655, 660, 663, 666, 668, 686, 693, 714, 717, 718, 723, 726, 730, 732, 735, 741, 773, 775, 784, 787, 789, 792, 813, 820, 837, 844, 848, 851, 852, 853, 854, 857, 858, 859, 860, 863, 864, 865, 866, 869, 870, 871, 873, 906, 907, 910, 913, 920, 921, 924, 925, 928, 929, 930, 931, 954, 955, 963, 967, 981, 982]
```
Low priority for now.
Oldest blob file is 587 but there are still blob files whose number is below 587.
Low priority for now. 

sst files are not deleted as well.
```bash
(base) ➜  with_model_gc_1.0_0.8 du -ch *.sst | grep total
1.7G    total

(base) ➜  with_model_gc_1.0_0.8 ll | grep sst | wc -l
540
```
[Status: Not started]

[Todo]
Multiple lifetime bucket.
[Status: Duplicate]

[Todo]
Reduce flush time.
[Status: Ongoing]

[Todo]
Save lots of time after removing training data logging and start
thread training model.
Use concurrent queue to add training data.
Another thread fetching data.
Start training in this training data fetching thread.
I think I can use reader writer queue.
```
https://github.com/cameron314/readerwriterqueue
```
I can use concurrent queue.
Let's tart implementing and then try.
db_bench_tool.cc
```cpp

    ThreadArg* arg = new ThreadArg[n];
      FLAGS_env->StartThread(ThreadBody, &arg[i]);

        ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
          // Start a new thread, invoking "function(arg)" within the new thread.
          // When "function(arg)" returns, the thread will be destroyed.
          virtual void StartThread(void (*function)(void* arg), void* arg) = 0;
```

db_impl_open.cc
```

  PeriodicTaskScheduler periodic_task_scheduler_;
  if (s.ok()) {
    s = impl->StartPeriodicTaskScheduler();
  }
        Status PeriodicTaskScheduler::Register(PeriodicTaskType task_type,
                                               const PeriodicTaskFunc& fn) {
          return Register(task_type, fn, kDefaultPeriodSeconds.at(task_type));
        }

```

```cpp
static void DataPumpThreadBody(void* arg) {
  DataPumpThread* t = reinterpret_cast<DataPumpThread*>(arg);
  DB* db = t->db;
  Random rnd(301);
  uint64_t i = 0;
  while (i++ < FLAGS_num_inserts) {
    if (!db->Put(WriteOptions(), Slice(rnd.RandomString(500)),
                 Slice(rnd.RandomString(500)))
             .ok()) {
      fprintf(stderr, "Error in put\n");
      exit(1);
    }
  }
}


```
[Status: Ongoing]

[Todo]
Use model to do regression prediction of lifetime of key and 
then put value to lifetime bucket that is close the predicted lifetime value. 

[Status: Ongoing]

[Todo]
adaptive model training .
No need to set label category.
Can just use regression task.
What kind of lifetime category should we have?
1M, 2M , 4M, 8M , 16 M ? For now.
But this is still classification problem.
Dont' know which one performs better when training model.
Let's try.
Add lifetime sequence vector storing lifetime of keys.
Update object from binary to regression.
Update ComputeBlobsMarkedForForcedGC() to get lifetime ttl from 
vector directly.
Update compaction iterator code to get lifetime prediction.
Need to bind number of lifetime bucket to size if lifetime sequence.
Not very high invalid_key_ratio after chaing model from binary classification
to regression.
Most of invalid_key_ratio values are around 0.2.

w-amp increases because of more gc jobs . 1.8 -> 2.3 Increases by 27%
blob size : 3.8 -> 3.6 . Drops 5%

[Status: Ongoing]
[Todo]
Count how many value is updated during compaction.
Do see blob index update count.
```
2024/03/09-12:34:00.402023 3824740 [db/compaction/compaction_iterator.cc:265] Num value blob index update: 49308
```
[Status: Done]

[Todo]
Need to create new branch when switch to regression model prediction.
Still do developmetno on main branch and create another branch for 
future comparison.
[Status: Done]


[Todo]
Adjust training params such as 
training threshold for number for sample.

Lifetime category range from [1M, 2M, 4M]

[Status: Not started]

[Todo]
Should we mention what's the size of db that just have valid keys and values?
[Status: Not started]

[Todo]
Consider move lifetime of blob to longer lifetime classification 
during gc.
Both w-amp and blob size are increased after this change.
Need to adjust creation time.
This idea is not good.
[Status: Done]





[Todo]
Model is bound the number of samples.
It would be better if we have more data to train model.
[Status: Not started]


[Todo]
When moving key from one lifetime bucket to another lifetime bucket,    
need to take creation time in to account.
Inherit the original creeation time of previous blob file 
when moveing lifetime bucket.
Tried it but not good..
wamp increases to 2.3
blob size does not change .
[Status: Done]

[Todo]
Don't recreate new model each time we train.
Use previous model and add more data to it.
[Status: Not started]

[Todo]
Would be good to do statistics about lifetime distribution
Andthen we can set up lifetime categories based on this distribution.
[Status: Not started]


[Todo]
Add training sample with probability that is inversely proportional to
the number of samples of that key.
```cpp
      if(past_distances_count > 0) {
        double inverse_distance = 1.0 / double(distance);
        // add training sample with inverse_distance probability
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1);
        double prob = dis(gen);
        if(prob < inverse_distance) {
          s = db_->AddTrainingSample(past_distances,
                                 blob_size,

```
BTW, this is not a good way to add training sample.
No wamp decrease or blob size decrease. 
Change to use edcs[0] as inverse probability to 
decide whether to add training sample.

Need to try no model one for comparison to show that modle improve
in terms of reducing wamp and space size of blob.
Oh fuck, no wamp increase or blob size increase after removing model. 
However, invalid_key_ratio is all 0.3 which is different to what we get from with model code.

Need to add total invalid count.
overall invalid key ratio during gc is 0.33 without model.
no model, 100k num ber sample triggering model training.
```
2024/03/10-15:08:03.608177 4033712 [db/compaction/garbage_collection_job.cc:146] gc input blob: 7970001, gc output blobs: 5272099, gc dropped blobs: 2697902, gc invalid key ratio: 0.339
```

with model
```
2024/03/10-15:44:11.177736 4135140 [db/compaction/garbage_collection_job.cc:146] gc input blob: 8440551, gc output blobs: 5613301, gc dropped blobs: 2827250, gc invalid key ratio: 0.335
```
Drop 100k more keys byt invalid key ratio is lower.
This is not good news.
What should we do? 
Switch to ycsb ?
Accumulate more training samples before we train model?
Tried 200k  but no improvement..
[Status: Ongoing]

[Todo]
Need to start testting read perf asap.
Let's run ycsb a b c d e for std rocksdb and mlsm.
[Status: Not started]


[Todo]
Add statistics for lifetime of keys whose past distance_count <= 1
And then we put future keys to lifetime bucket based on this statistics.
[Status: Not started]

[Todo]
Do another prediction during gc?
So now we don't need to do prediction during flush. 
This is a good idea.
Let's do it.
But this doesn't make sense. 
model has already given prediction for this value.
If this value is not invalidated, then it does 
not make sense to do another prediction.
[Status: Not started]


[Todo]
Test ycsb a
Need to compare my results with std rocksdb and other kv sep implementation.
[Status: Ongoing]

[Todo]
Need to close sst files and blob files.
Is this because of internal iterator?
```cpp
    status = compaction_job.Install(*c->mutable_cf_options());
    c->ReleaseCompactionFiles(status);
```

```cpp
      result->reset(new PosixRandomAccessFile(
          fname, fd, GetLogicalBlockSizeForReadIfNeeded(options, fname, fd),
          options
#if defined(ROCKSDB_IOURING_PRESENT)
          ,
          !IsIOUringEnabled() ? nullptr : thread_local_io_urings_.get()
#endif
 
PosixRandomAccessFile::~PosixRandomAccessFile() { close(fd_); }
```


```
1134.sst
2024/03/10-21:34:53.301925 975830 [db/compaction/compaction_job.cc:2091] [default] [JOB 328] Compacting 1@1 + 2@2 files to L2, score 1.49
2024/03/10-21:34:53.301933 975830 [db/compaction/compaction_job.cc:2097] [default]: Compaction start summary: Base version 323 Base level 1, inputs: [1097(2938KB)], [845(4152KB) 1041(3129KB)]
2024/03/10-21:34:53.301949 975830 EVENT_LOG_v1 {"time_micros": 1710077693301939, "job": 328, "event": "compaction_started", "compaction_reason": "LevelMaxLevelSize", "files_L1": [1097], "files_L2": [845, 1041], "gc_blob_files": [], "score": 1.4931, "input_data_size": 10465712, "oldest_snapshot_seqno": -1}
2024/03/10-21:34:53.302036 975830 [db/compaction/compaction_iterator.cc:252] blob_age_cutoff_file_number (18446744073709551615) enable_blob_garbage_collection (0) 
2024/03/10-21:34:53.535341 975830 [db/compaction/compaction_job.cc:1689] [default] [JOB 328] Generated table #1133: 22018 keys, 6082789 bytes, temperature: kUnknown
2024/03/10-21:34:53.535387 975830 EVENT_LOG_v1 {"time_micros": 1710077693535362, "cf_name": "default", "job": 328, "event": "table_file_creation", "file_number": 1133, "file_size": 6082789, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 1139625, "largest_seqno": 2767474, "table_properties": {"data_size": 6040247, "index_size": 13888, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 27589, "raw_key_size": 5812752, "raw_average_key_size": 264, "raw_value_size": 242198, "raw_average_value_size": 11, "num_data_blocks": 760, "num_entries": 22018, "num_filter_entries": 22018, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1710077457, "oldest_key_time": 0, "file_creation_time": 1710077693, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "3e390d9c-d547-4513-80a5-2e23e3802471", "db_session_id": "G2JDQIDYPPYU3U8Q499A", "orig_file_number": 1133, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 356}
2024/03/10-21:34:53.669593 975830 [db/compaction/compaction_job.cc:1689] [default] [JOB 328] Generated table #1134: 12846 keys, 3549159 bytes, temperature: kUnknown
2024/03/10-21:34:53.669638 975830 EVENT_LOG_v1 {"time_micros": 1710077693669614, "cf_name": "default", "job": 328, "event": "table_file_creation", "file_number": 1134, "file_size": 3549159, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 1139700, "largest_seqno": 2766760, "table_properties": {"data_size": 3524030, "index_size": 7999, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 16069, "raw_key_size": 3391344, "raw_average_key_size": 264, "raw_value_size": 141306, "raw_average_value_size": 11, "num_data_blocks": 443, "num_entries": 12846, "num_filter_entries": 12846, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1710077457, "oldest_key_time": 0, "file_creation_time": 1710077693, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "3e390d9c-d547-4513-80a5-2e23e3802471", "db_session_id": "G2JDQIDYPPYU3U8Q499A", "orig_file_number": 1134, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 356}
2024/03/10-21:34:53.703249 975830 [db/compaction/compaction_job.cc:1689] [default] [JOB 328] Generated table #1135: 2979 keys, 826766 bytes, temperature: kUnknown
2024/03/10-21:34:53.703287 975830 EVENT_LOG_v1 {"time_micros": 1710077693703264, "cf_name": "default", "job": 328, "event": "table_file_creation", "file_number": 1135, "file_size": 826766, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 2604767, "largest_seqno": 2767505, "table_properties": {"data_size": 820226, "index_size": 1703, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 3781, "raw_key_size": 786456, "raw_average_key_size": 264, "raw_value_size": 32769, "raw_average_value_size": 11, "num_data_blocks": 103, "num_entries": 2979, "num_filter_entries": 2979, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1710077457, "oldest_key_time": 0, "file_creation_time": 1710077693, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "3e390d9c-d547-4513-80a5-2e23e3802471", "db_session_id": "G2JDQIDYPPYU3U8Q499A", "orig_file_number": 1135, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 1040}
2024/03/10-21:34:53.703336 975830 [db/compaction/compaction_iterator.cc:268] Num value blob index update: 0




2024/03/10-21:34:54.736895 975830 EVENT_LOG_v1 {"time_micros": 1710077694736885, "job": 332, "event": "compaction_started", "compaction_reason": "LevelMaxLevelSize", "files_L2": [1134], "files_L3": [323], "gc_blob_files": [], "score": 1.18362, "input_data_size": 6030493, "oldest_snapshot_seqno": -1}
2024/03/10-21:34:54.736982 975830 [db/compaction/compaction_iterator.cc:252] blob_age_cutoff_file_number (18446744073709551615) enable_blob_garbage_collection (0) 
2024/03/10-21:34:54.871139 975830 [db/compaction/compaction_job.cc:1689] [default] [JOB 332] Generated table #1145: 11978 keys, 3306919 bytes, temperature: kUnknown
2024/03/10-21:34:54.871182 975830 EVENT_LOG_v1 {"time_micros": 1710077694871158, "cf_name": "default", "job": 332, "event": "table_file_creation", "file_number": 1145, "file_size": 3306919, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 1992, "largest_seqno": 2766760, "table_properties": {"data_size": 3283324, "index_size": 7553, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 14981, "raw_key_size": 3162192, "raw_average_key_size": 264, "raw_value_size": 131758, "raw_average_value_size": 11, "num_data_blocks": 414, "num_entries": 11978, "num_filter_entries": 11978, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1710077299, "oldest_key_time": 0, "file_creation_time": 1710077694, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "3e390d9c-d547-4513-80a5-2e23e3802471", "db_session_id": "G2JDQIDYPPYU3U8Q499A", "orig_file_number": 1145, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 156}
2024/03/10-21:34:54.980698 975830 [db/compaction/compaction_job.cc:1689] [default] [JOB 332] Generated table #1146: 9860 keys, 2722328 bytes, temperature: kUnknown
2024/03/10-21:34:54.980745 975830 EVENT_LOG_v1 {"time_micros": 1710077694980720, "cf_name": "default", "job": 332, "event": "table_file_creation", "file_number": 1146, "file_size": 2722328, "file_checksum": "", "file_checksum_func_name": "Unknown", "smallest_seqno": 887, "largest_seqno": 2766104, "table_properties": {"data_size": 2702749, "index_size": 6161, "index_partitions": 0, "top_level_index_size": 0, "index_key_is_user_key": 1, "index_value_is_delta_encoded": 1, "filter_size": 12357, "raw_key_size": 2603040, "raw_average_key_size": 264, "raw_value_size": 108460, "raw_average_value_size": 11, "num_data_blocks": 340, "num_entries": 9860, "num_filter_entries": 9860, "num_deletions": 0, "num_merge_operands": 0, "num_range_deletions": 0, "format_version": 0, "fixed_key_len": 0, "filter_policy": "bloomfilter", "column_family_name": "default", "column_family_id": 0, "comparator": "leveldb.BytewiseComparator", "merge_operator": "PutOperator", "prefix_extractor_name": "nullptr", "property_collectors": "[]", "compression": "NoCompression", "compression_options": "window_bits=-14; level=32767; strategy=0; max_dict_bytes=0; zstd_max_train_bytes=0; enabled=0; max_dict_buffer_bytes=0; use_zstd_dict_trainer=1; ", "creation_time": 1710077299, "oldest_key_time": 0, "file_creation_time": 1710077694, "slow_compression_estimated_data_size": 0, "fast_compression_estimated_data_size": 0, "db_id": "3e390d9c-d547-4513-80a5-2e23e3802471", "db_session_id": "G2JDQIDYPPYU3U8Q499A", "orig_file_number": 1146, "seqno_to_time_mapping": "N/A"}, "oldest_blob_file_number": 156}
2024/03/10-21:34:54.980799 975830 [db/compaction/compaction_iterator.cc:268] Num value blob index update: 5755
2024/03/10-21:34:54.999595 975830 (Original Log Time 2024/03/10-21:34:54.981224) [db/compaction/compaction_job.cc:1762] [default] [JOB 332] Compacted 1@2 + 1@3 files to L3 => 6029247 bytes
2024/03/10-21:34:54.999602 975830 (Original Log Time 2024/03/10-21:34:54.998996) [db/compaction/compaction_job.cc:867] [default] compacted to: files[0 9 73 169 0 0 0 0] max score 1.17, MB/sec: 24.7 rd, 24.7 wr, level 3, files in(1, 1) out(2 +0 blob) MB in(3.4, 2.4 +0.0 blob) out(5.7 +0.0 blob), read-write-amplify(3.4) write-amplify(1.7) OK, records in: 21838, records dropped: 0 output_compression: NoCompression
2024/03/10-21:34:54.999605 975830 (Original Log Time 2024/03/10-21:34:54.999001) [db/compaction/compaction_job.cc:893] [default] Blob file summary: head=156, tail=1126
2024/03/10-21:34:54.999614 975830 (Original Log Time 2024/03/10-21:34:54.999122) EVENT_LOG_v1 {"time_micros": 1710077694999008, "job": 332, "event": "compaction_finished", "compaction_time_micros": 243915, "compaction_time_cpu_micros": 241159, "output_level": 3, "num_output_files": 2, "total_output_size": 6029247, "num_input_records": 21838, "num_output_records": 21838, "num_subcompactions": 1, "output_compression": "NoCompression", "num_single_delete_mismatches": 0, "num_single_delete_fallthrough": 0, "lsm_state": [0, 9, 73, 169, 0, 0, 0, 0], "blob_file_head": 156, "blob_file_tail": 1126, "blob_files": [156, 157, 224, 225, 228, 229, 295, 296, 299, 300, 356, 357, 360, 361, 426, 427, 430, 431, 495, 496, 499, 500, 565, 566, 569, 570, 624, 625, 628, 629, 689, 691, 694, 695, 696, 743, 745, 759, 771, 772, 775, 776, 778, 825, 827, 842, 871, 874, 879, 880, 881, 926, 927, 943, 967, 974, 977, 978, 980, 1027, 1029, 1040, 1070, 1072, 1080, 1081, 1083, 1126], "garbage_count": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], "total_blob_count": [80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 490, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 489, 80909, 488, 80909, 489, 80909, 489, 80909, 80909, 489, 488, 80909, 489, 80909, 489, 80909, 80909, 488, 489, 80909, 489, 80909, 489, 80909, 80909, 489, 488, 80909, 489, 80909, 489, 80909, 80909, 489, 489, 80909, 486, 80909, 488], "lifetime_blob_0": [], "lifetime_blob_1": [156, 157, 224, 225, 228, 229, 295, 296, 299, 300, 356, 357, 360, 361, 426, 427, 430, 431, 495, 496, 499, 500, 565, 566, 569, 570, 624, 625, 628, 629, 689, 691, 694, 695, 759, 772, 775, 776, 842, 874, 879, 880, 943, 974, 977, 978, 1040, 1070, 1080, 1081], "lifetime_blob_2": [696, 743, 745, 771, 778, 825, 827, 871, 881, 926, 927, 967, 980, 1027, 1029, 1072, 1083, 1126], "lifetime_blob_3": []}
```

1134.sst is not deleted


```
2024/03/10-21:34:55.324849 975830 EVENT_LOG_v1 {"time_micros": 1710077695324839, "job": 334, "event": "compaction_started", "compaction_reason": "LevelMaxLevelSize", "files_L2": [1133], "files_L3": [353, 322], "gc_blob_files": [], "score": 1.14424, "input_data_size": 10363723, "oldest_snapshot_seqno": -1}

2024/03/10-21:34:57.760581 975831 EVENT_LOG_v1 {"time_micros": 1710077697760579, "job": 330, "event": "table_file_deletion", "file_number": 353}
```


Number of version is ok
```
(gdb) p default_cf_handle_->cfd()->GetSuperVersion()->current
$5 = (rocksdb::Version *) 0x1552f55d8840
(gdb) p default_cf_handle_->cfd()->GetSuperVersion()->current->prev_
$6 = (rocksdb::Version *) 0x555556a47090
(gdb) p default_cf_handle_->cfd()->GetSuperVersion()->current->prev_->prev_
$7 = (rocksdb::Version *) 0x15532b38bdd0
```
Maybe there is problem with refs of file.

When are sst files deleted?
sst files are not in version but they are not deleted either.
```
(gdb)  p default_cf_handle_->cfd()->GetSuperVersion()->current->refs_
$10 = 4
```
Where are sst files deleted?
```cpp
  void ReleaseCompactionFiles(const Status& status) {
    for (auto& f : compaction_files_) {
      if (f.file) {
        f.file->file()->Close();
        f.file->Unref();
      }
    }
    compaction_files_.clear();
    compaction_files_.reserve(2);
  }
```

When fd is closed? 
```
FileMetaData{
  FileDescriptor fd;
}
```

```

          file_meta->fd.table_reader = table_cache_->get_cache().Value(handle);
```

Should check this code and see when table file is deleted.
```cpp

Status BlockBasedTable::Open(
    const ReadOptions& read_options, const ImmutableOptions& ioptions,
    const EnvOptions& env_options, const BlockBasedTableOptions& table_options,
    const InternalKeyComparator& internal_comparator,
    std::unique_ptr<RandomAccessFileReader>&& file, uint64_t file_size,
    std::unique_ptr<TableReader>* table_reader,
    std::shared_ptr<CacheReservationManager> table_reader_cache_res_mgr,
    const std::shared_ptr<const SliceTransform>& prefix_extractor,
    const bool prefetch_index_and_filter_in_cache, const bool skip_filters,
    const int level, const bool immortal_table,
    const SequenceNumber largest_seqno, const bool force_direct_prefetch,
    TailPrefetchStats* tail_prefetch_stats,
    BlockCacheTracer* const block_cache_tracer,
    size_t max_file_size_for_l0_meta_pin, const std::string& cur_db_session_id,
    uint64_t cur_file_num, UniqueId64x2 expected_unique_id) {
  table_reader->reset();

  Status s;
  Footer footer;
  std::unique_ptr<FilePrefetchBuffer> prefetch_buffer;

  // From read_options, retain deadline, io_timeout, rate_limiter_priority, and
  // verify_checksums. In future, we may retain more options.
  ReadOptions ro;
  ro.deadline = read_options.deadline;
  ro.io_timeout = read_options.io_timeout;
  ro.rate_limiter_priority = read_options.rate_limiter_priority;
  ro.verify_checksums = read_options.verify_checksums;

  // prefetch both index and filters, down to all partitions
  const bool prefetch_all = prefetch_index_and_filter_in_cache || level == 0;
  const bool preload_all = !table_options.cache_index_and_filter_blocks;

  if (!ioptions.allow_mmap_reads) {
    s = PrefetchTail(ro, file.get(), file_size, force_direct_prefetch,
                     tail_prefetch_stats, prefetch_all, preload_all,
                     &prefetch_buffer);
    // Return error in prefetch path to users.
    if (!s.ok()) {
      return s;
    }
  } else {
    // Should not prefetch for mmap mode.
    prefetch_buffer.reset(new FilePrefetchBuffer(
        0 /* readahead_size */, 0 /* max_readahead_size */, false /* enable */,
        true /* track_min_offset */));
  }

  // Read in the following order:
  //    1. Footer
  //    2. [metaindex block]
  //    3. [meta block: properties]
  //    4. [meta block: range deletion tombstone]
  //    5. [meta block: compression dictionary]
  //    6. [meta block: index]
  //    7. [meta block: filter]
  IOOptions opts;
  s = file->PrepareIOOptions(ro, opts);
  if (s.ok()) {
    s = ReadFooterFromFile(opts, file.get(), *ioptions.fs,
                           prefetch_buffer.get(), file_size, &footer,
                           kBlockBasedTableMagicNumber);
  }
  if (!s.ok()) {
    return s;
  }
  if (!IsSupportedFormatVersion(footer.format_version())) {
    return Status::Corruption(
        "Unknown Footer version. Maybe this file was created with newer "
        "version of RocksDB?");
  }

  BlockCacheLookupContext lookup_context{TableReaderCaller::kPrefetch};
  Rep* rep = new BlockBasedTable::Rep(ioptions, env_options, table_options,
                                      internal_comparator, skip_filters,
                                      file_size, level, immortal_table);
  rep->file = std::move(file);
  rep->footer = footer;

  // For fully portable/stable cache keys, we need to read the properties
  // block before setting up cache keys. TODO: consider setting up a bootstrap
  // cache key for PersistentCache to use for metaindex and properties blocks.
  rep->persistent_cache_options = PersistentCacheOptions();

  // Meta-blocks are not dictionary compressed. Explicitly set the dictionary
  // handle to null, otherwise it may be seen as uninitialized during the below
  // meta-block reads.
  rep->compression_dict_handle = BlockHandle::NullBlockHandle();

  // Read metaindex
  std::unique_ptr<BlockBasedTable> new_table(
      new BlockBasedTable(rep, block_cache_tracer));
  std::unique_ptr<Block> metaindex;
  std::unique_ptr<InternalIterator> metaindex_iter;
  s = new_table->ReadMetaIndexBlock(ro, prefetch_buffer.get(), &metaindex,
                                    &metaindex_iter);
  if (!s.ok()) {
    return s;
  }

  // Populates table_properties and some fields that depend on it,
  // such as index_type.
  s = new_table->ReadPropertiesBlock(ro, prefetch_buffer.get(),
                                     metaindex_iter.get(), largest_seqno);
  if (!s.ok()) {
    return s;
  }

  // Populate BlockCreateContext
  bool blocks_definitely_zstd_compressed =
      rep->table_properties &&
      (rep->table_properties->compression_name ==
           CompressionTypeToString(kZSTD) ||
       rep->table_properties->compression_name ==
           CompressionTypeToString(kZSTDNotFinalCompression));
  rep->create_context =
      BlockCreateContext(&rep->table_options, rep->ioptions.stats,
                         blocks_definitely_zstd_compressed);

  // Check expected unique id if provided
  if (expected_unique_id != kNullUniqueId64x2) {
    auto props = rep->table_properties;
    if (!props) {
      return Status::Corruption("Missing table properties on file " +
                                std::to_string(cur_file_num) +
                                " with known unique ID");
    }
    UniqueId64x2 actual_unique_id{};
    s = GetSstInternalUniqueId(props->db_id, props->db_session_id,
                               props->orig_file_number, &actual_unique_id,
                               /*force*/ true);
    assert(s.ok());  // because force=true
    if (expected_unique_id != actual_unique_id) {
      return Status::Corruption(
          "Mismatch in unique ID on table file " +
          std::to_string(cur_file_num) +
          ". Expected: " + InternalUniqueIdToHumanString(&expected_unique_id) +
          " Actual: " + InternalUniqueIdToHumanString(&actual_unique_id));
    }
    TEST_SYNC_POINT_CALLBACK("BlockBasedTable::Open::PassedVerifyUniqueId",
                             &actual_unique_id);
  } else {
    TEST_SYNC_POINT_CALLBACK("BlockBasedTable::Open::SkippedVerifyUniqueId",
                             nullptr);
    if (ioptions.verify_sst_unique_id_in_manifest && ioptions.logger) {
      // A crude but isolated way of reporting unverified files. This should not
      // be an ongoing concern so doesn't deserve a place in Statistics IMHO.
      static std::atomic<uint64_t> unverified_count{0};
      auto prev_count =
          unverified_count.fetch_add(1, std::memory_order_relaxed);
      if (prev_count == 0) {
        ROCKS_LOG_WARN(
            ioptions.logger,
            "At least one SST file opened without unique ID to verify: %" PRIu64
            ".sst",
            cur_file_num);
      } else if (prev_count % 1000 == 0) {
        ROCKS_LOG_WARN(
            ioptions.logger,
            "Another ~1000 SST files opened without unique ID to verify");
      }
    }
  }

  // Set up prefix extracto as needed
  bool force_null_table_prefix_extractor = false;
  TEST_SYNC_POINT_CALLBACK(
      "BlockBasedTable::Open::ForceNullTablePrefixExtractor",
      &force_null_table_prefix_extractor);
  if (force_null_table_prefix_extractor) {
    assert(!rep->table_prefix_extractor);
  } else if (!PrefixExtractorChangedHelper(rep->table_properties.get(),
                                           prefix_extractor.get())) {
    // Establish fast path for unchanged prefix_extractor
    rep->table_prefix_extractor = prefix_extractor;
  } else {
    // Current prefix_extractor doesn't match table
    if (rep->table_properties) {
      //**TODO: If/When the DBOptions has a registry in it, the ConfigOptions
      // will need to use it
      ConfigOptions config_options;
      Status st = SliceTransform::CreateFromString(
          config_options, rep->table_properties->prefix_extractor_name,
          &(rep->table_prefix_extractor));
      if (!st.ok()) {
        //**TODO: Should this be error be returned or swallowed?
        ROCKS_LOG_ERROR(rep->ioptions.logger,
                        "Failed to create prefix extractor[%s]: %s",
                        rep->table_properties->prefix_extractor_name.c_str(),
                        st.ToString().c_str());
      }
    }
  }

  // With properties loaded, we can set up portable/stable cache keys
  SetupBaseCacheKey(rep->table_properties.get(), cur_db_session_id,
                    cur_file_num, &rep->base_cache_key);

  rep->persistent_cache_options =
      PersistentCacheOptions(rep->table_options.persistent_cache,
                             rep->base_cache_key, rep->ioptions.stats);

  s = new_table->ReadRangeDelBlock(ro, prefetch_buffer.get(),
                                   metaindex_iter.get(), internal_comparator,
                                   &lookup_context);
  if (!s.ok()) {
    return s;
  }
  s = new_table->PrefetchIndexAndFilterBlocks(
      ro, prefetch_buffer.get(), metaindex_iter.get(), new_table.get(),
      prefetch_all, table_options, level, file_size,
      max_file_size_for_l0_meta_pin, &lookup_context);

  if (s.ok()) {
    // Update tail prefetch stats
    assert(prefetch_buffer.get() != nullptr);
    if (tail_prefetch_stats != nullptr) {
      assert(prefetch_buffer->min_offset_read() < file_size);
      tail_prefetch_stats->RecordEffectiveSize(
          static_cast<size_t>(file_size) - prefetch_buffer->min_offset_read());
    }
  }

  if (s.ok() && table_reader_cache_res_mgr) {
    std::size_t mem_usage = new_table->ApproximateMemoryUsage();
    s = table_reader_cache_res_mgr->MakeCacheReservation(
        mem_usage, &(rep->table_reader_cache_res_handle));
    if (s.IsMemoryLimit()) {
      s = Status::MemoryLimit(
          "Can't allocate " +
          kCacheEntryRoleToCamelString[static_cast<std::uint32_t>(
              CacheEntryRole::kBlockBasedTableReader)] +
          " due to memory limit based on "
          "cache capacity for memory allocation");
    }
  }

  if (s.ok()) {
    *table_reader = std::move(new_table);
  }
  return s;
}
```

```cpp

void DBImpl::PurgeObsoleteFiles(JobContext& state, bool schedule_only) {
or 

void DBImpl::SchedulePurge() {
  mutex_.AssertHeld();
  assert(opened_successfully_);

  // Purge operations are put into High priority queue
  bg_purge_scheduled_++;
  env_->Schedule(&DBImpl::BGWorkPurge, this, Env::Priority::HIGH, nullptr);
}
void DBImpl::BackgroundCallPurge() {

// Delete obsolete files and log status and information of file deletion
void DBImpl::DeleteObsoleteFileImpl(int job_id, const std::string& fname,
                                    const std::string& path_to_sync,
                                    FileType type, uint64_t number) {
  TEST_SYNC_POINT_CALLBACK("DBImpl::DeleteObsoleteFileImpl::BeforeDeletion",
                           const_cast<std::string*>(&fname));

  Status file_deletion_status;
  if (type == kTableFile || type == kBlobFile || type == kWalFile) {
    // Rate limit WAL deletion only if its in the DB dir
    file_deletion_status = DeleteDBFile(
        &immutable_db_options_, fname, path_to_sync,
        /*force_bg=*/false,
        /*force_fg=*/(type == kWalFile) ? !wal_in_db_path_ : false);
  } else {
    file_deletion_status = env_->DeleteFile(fname);
  }
  TEST_SYNC_POINT_CALLBACK("DBImpl::DeleteObsoleteFileImpl:AfterDeletion",
                           &file_deletion_status);
  TEST_SYNC_POINT_CALLBACK("DBImpl::DeleteObsoleteFileImpl:AfterDeletion2",
                           const_cast<std::string*>(&fname));
  if (file_deletion_status.ok()) {
    ROCKS_LOG_DEBUG(immutable_db_options_.info_log,
                    "[JOB %d] Delete %s type=%d #%" PRIu64 " -- %s\n", job_id,
                    fname.c_str(), type, number,
                    file_deletion_status.ToString().c_str());
  } else if (env_->FileExists(fname).IsNotFound()) {
    ROCKS_LOG_INFO(
        immutable_db_options_.info_log,
        "[JOB %d] Tried to delete a non-existing file %s type=%d #%" PRIu64
        " -- %s\n",
        job_id, fname.c_str(), type, number,
        file_deletion_status.ToString().c_str());
  } else {
    ROCKS_LOG_ERROR(immutable_db_options_.info_log,
                    "[JOB %d] Failed to delete %s type=%d #%" PRIu64 " -- %s\n",
                    job_id, fname.c_str(), type, number,
                    file_deletion_status.ToString().c_str());
  }
  if (type == kTableFile) {
    EventHelpers::LogAndNotifyTableFileDeletion(
        &event_logger_, job_id, number, fname, file_deletion_status, GetName(),
        immutable_db_options_.listeners);
  }
  if (type == kBlobFile) {
    EventHelpers::LogAndNotifyBlobFileDeletion(
        &event_logger_, immutable_db_options_.listeners, job_id, number, fname,
        file_deletion_status, GetName());
  }
}

void EventHelpers::LogAndNotifyTableFileDeletion(
    EventLogger* event_logger, int job_id, uint64_t file_number,
    const std::string& file_path, const Status& status,
    const std::string& dbname,
    const std::vector<std::shared_ptr<EventListener>>& listeners) {
  JSONWriter jwriter;
  AppendCurrentTime(&jwriter);

  jwriter << "job" << job_id << "event"
          << "table_file_deletion"
          << "file_number" << file_number;
  if (!status.ok()) {
    jwriter << "status" << status.ToString();
  }

  jwriter.EndObject();

  event_logger->Log(jwriter);

  if (listeners.empty()) {
    return;
  }
  TableFileDeletionInfo info;
  info.db_name = dbname;
  info.job_id = job_id;
  info.file_path = file_path;
  info.status = status;
  for (auto& listener : listeners) {
    listener->OnTableFileDeleted(info);
  }
  info.status.PermitUncheckedError();
}
```


Need to make sure obsolete file are deleted during gc job as well.
I think that once gc job skip obsolete file deletion, it will not be deleted again.
```cpp
void DBImpl::BackgroundCallGarbageCollection(PrepickedCompaction* compaction,
                                             Env::Priority thread_pri) {
 // If compaction failed, we want to delete all temporary files that we
    // might have created (they might not be all recorded in job_context in
    // case of a failure). Thus, we force full scan in FindObsoleteFiles()
    FindObsoleteFiles(&job_context, !s.ok() && !s.IsShutdownInProgress() &&
                                        !s.IsManualCompactionPaused() &&
                                        !s.IsColumnFamilyDropped() &&
                                        !s.IsBusy());
    TEST_SYNC_POINT("DBImpl::BackgroundGarbageCollection:FoundObsoleteFiles");

    // delete unnecessary files if any, this is done outside the mutex
    if (job_context.HaveSomethingToClean() ||
        job_context.HaveSomethingToDelete() || !log_buffer.IsEmpty()) {

          inline bool HaveSomethingToDelete() const {
            return !(full_scan_candidate_files.empty() && sst_delete_files.empty() &&
                     blob_delete_files.empty() && log_delete_files.empty() &&
                     manifest_delete_files.empty());
          }


      mutex_.Unlock();
      // Have to flush the info logs before bg_compaction_scheduled_--
      // because if bg_flush_scheduled_ becomes 0 and the lock is
      // released, the deconstructor of DB can kick in and destroy all the
      // states of DB so info_log might not be available after that point.
      // It also applies to access other states that DB owns.
      log_buffer.FlushBufferToLog();
      if (job_context.HaveSomethingToDelete()) {
        PurgeObsoleteFiles(job_context);
        TEST_SYNC_POINT("DBImpl::BackgroundCallCompaction:PurgedObsoleteFiles");
      }
      job_context.Clean();
      mutex_.Lock();
    }
```
comment iter crreatino part of compaction job . Because compaction doesn't 
use iter in this version.


FindObsoleteFiles() is called after each compaction job.
Did I do this in garbagecollection job?
First FindObsoleteFiles(), and then PurgeObsoleteFiles()
When and where pending_outputs_ is updated in compaction jobs?
I don't rememeber I did any of these steps in my gc job .
Need to check and make sure I copy these functions to my gc job function.
Fuck, it's not easy to do system coding.
It's just complicated.
```cpp
void DBImpl::FindObsoleteFiles(JobContext* job_context, bool force,
                               bool no_full_scan) {
  mutex_.AssertHeld();

  // don't delete files that might be currently written to from compaction
  // threads
  // Since job_context->min_pending_output is set, until file scan finishes,
  // mutex_ cannot be released. Otherwise, we might see no min_pending_output
  // here but later find newer generated unfinalized files while scanning.
  job_context->min_pending_output = MinObsoleteSstNumberToKeep();

  // Get obsolete files.  This function will also update the list of
  // pending files in VersionSet().
  versions_->GetObsoleteFiles(
      &job_context->sst_delete_files, &job_context->blob_delete_files,
      &job_context->manifest_delete_files, job_context->min_pending_output);



    uint64_t DBImpl::MinObsoleteSstNumberToKeep() {
      mutex_.AssertHeld();
      if (!pending_outputs_.empty()) {
        return *pending_outputs_.begin();
      }
      return std::numeric_limits<uint64_t>::max();
    }
```

Do I miss add new blob file number to pending_outputs_ during gc?
Is this the root cause?
```cpp

void DBImpl::BackgroundCallCompaction(PrepickedCompaction* prepicked_compaction,

    std::unique_ptr<std::list<uint64_t>::iterator>
        pending_outputs_inserted_elem(new std::list<uint64_t>::iterator(
            CaptureCurrentFileNumberInPendingOutputs()));


            DBImpl::CaptureCurrentFileNumberInPendingOutputs() {
              // We need to remember the iterator of our insert, because after the
              // background job is done, we need to remove that element from
              // pending_outputs_.
              pending_outputs_.push_back(versions_->current_next_file_number());
              auto pending_outputs_inserted_elem = pending_outputs_.end();
              --pending_outputs_inserted_elem;
              return pending_outputs_inserted_elem;
            }


    Status s = BackgroundCompaction(&made_progress, &job_context, &log_buffer,

    ReleaseFileNumberFromPendingOutputs(pending_outputs_inserted_elem);
        void DBImpl::ReleaseFileNumberFromPendingOutputs(
            std::unique_ptr<std::list<uint64_t>::iterator>& v) {
          if (v.get() != nullptr) {
            pending_outputs_.erase(*v.get());
            v.reset();
          }
        }

    // If compaction failed, we want to delete all temporary files that we
    // might have created (they might not be all recorded in job_context in
    // case of a failure). Thus, we force full scan in FindObsoleteFiles()
    FindObsoleteFiles(&job_context, !s.ok() && !s.IsShutdownInProgress() &&
                                        !s.IsManualCompactionPaused() &&
                                        !s.IsColumnFamilyDropped() &&
                                        !s.IsBusy());
          job_context->min_pending_output = MinObsoleteSstNumberToKeep();

          // Get obsolete files.  This function will also update the list of
          // pending files in VersionSet().
          versions_->GetObsoleteFiles(
              &job_context->sst_delete_files, &job_context->blob_delete_files,
              &job_context->manifest_delete_files, job_context->min_pending_output);

                void VersionSet::GetObsoleteFiles(std::vector<ObsoleteFileInfo>* files,
                  std::vector<ObsoleteFileInfo> pending_files;
                  for (auto& f : obsolete_files_) {
                    if (f.metadata->fd.GetNumber() < min_pending_output) {
                      files->emplace_back(std::move(f));
                    } else {
                      pending_files.emplace_back(std::move(f));
                    }
                  }
                  obsolete_files_.swap(pending_files);



 
    if (job_context.HaveSomethingToClean() ||
        job_context.HaveSomethingToDelete() || !log_buffer.IsEmpty()) {
      mutex_.Unlock();
     
        PurgeObsoleteFiles(job_context);
```


obsolete_files_ are updated in destructor of Version.
Still don't understand why sst files are not deleted.
It should be deleted after destructor of version.
version builder can unref file and make f->refs <=0 .
But it does not put f to verions_set_ obsoleted_files_ .
```cpp
Version::~Version() {
  assert(refs_ == 0);

  // Remove from linked list
  prev_->next_ = next_;
  next_->prev_ = prev_;

  // Drop references to files
  for (int level = 0; level < storage_info_.num_levels_; level++) {
    for (size_t i = 0; i < storage_info_.files_[level].size(); i++) {
      FileMetaData* f = storage_info_.files_[level][i];
      assert(f->refs > 0);
      f->refs--;
      if (f->refs <= 0) {
        assert(cfd_ != nullptr);
        uint32_t path_id = f->fd.GetPathId();
        assert(path_id < cfd_->ioptions()->cf_paths.size());
        vset_->obsolete_files_.push_back(
            ObsoleteFileInfo(f, cfd_->ioptions()->cf_paths[path_id].path,
                             cfd_->GetFileMetadataCacheReservationManager()));
      }
    }
  }
}
```



obsolete_blob_files_ are updated in AddObsolteBlobFileWithLifetime()    
```cpp
    auto deleter = [vs, ioptions](SharedBlobFileMetaData* shared_meta) {
      if (vs) {
        assert(ioptions);
        assert(!ioptions->cf_paths.empty());
        assert(shared_meta);

        vs->AddObsoleteBlobFileWithLifetime(shared_meta->GetBlobFileNumber(),
                                            ioptions->cf_paths.front().path,
                                            shared_meta->GetLifetimeLabel());
        // vs->AddObsoleteBlobFile(shared_meta->GetBlobFileNumber(),
        //                         ioptions->cf_paths.front().path);
      }

      delete shared_meta;
    };


  void AddObsoleteBlobFileWithLifetime(uint64_t blob_file_number, std::string path,
                                      uint64_t lifetime) {
    assert(table_cache_);

    table_cache_->Erase(GetSliceForKey(&blob_file_number));

    obsolete_blob_files_.emplace_back(blob_file_number, std::move(path));
  }


```

There are no full scan to delete obsolete files in gc job.
And there are problems with our current gc job that causes
obsolete files are not deleted.
I guess we can solve current problem by force full scan.

Let's just change gc job code and make sure we do full scan to delete obsolete files.

Add 
```cpp
    ReleaseFileNumberFromPendingOutputs(pending_outputs_inserted_elem);
```
to BackgroundCallGarbageCollection()
Hope this can work.
Otherwise , I will do full scan to delete oboslte files.
I think we fix this issue after  adding ReleaseFileNumberFromPendingOutputs()
Still got too many open files error after 9783k writes.
Total number of sst: 896
Total sst file: 
```
(base) ➜  with_gc_1.0_0.8 ll | grep sst | wc -l
935
```


Total blob file: 231
```
(base) ➜  with_gc_1.0_0.8 ll | grep blob | wc -l
238
```

When close(fd) is called?
```cpp
IOStatus NewRandomAccessFile(const std::string& fname,
                           const FileOptions& options,
                           std::unique_ptr<FSRandomAccessFile>* result,
                           IODebugContext* /*dbg*/) override {

    class PosixRandomAccessFile : public FSRandomAccessFile {

        PosixRandomAccessFile::~PosixRandomAccessFile() { close(fd_); }
```

NewRandomAccessFile is called in too many functions.
blob_file_reader.cc
```

Status BlobFileReader::OpenFile(
  std::unique_ptr<FSRandomAccessFile> file;
        fs->NewRandomAccessFile(blob_file_path, file_opts, &file, dbg);
```


table_cache.cc
```cpp
Status TableCache::GetTableReader(
  std::string fname = TableFileName(
      ioptions_.cf_paths, file_meta.fd.GetNumber(), file_meta.fd.GetPathId());
  std::unique_ptr<FSRandomAccessFile> file;

    s = ioptions_.fs->NewRandomAccessFile(fname, fopts, &file, nullptr);
```

version_set.cc
```cpp
Status Version::GetTableProperties(std::shared_ptr<const TableProperties>* tp,
                                   const FileMetaData* file_meta,
                                   const std::string* fname) const {
  std::unique_ptr<FSRandomAccessFile> file;
  s = ioptions->fs->NewRandomAccessFile(file_name, file_options_, &file,
```


random_access_file_reader.cc
```cpp
Status BlobFile::ReadMetadata(const std::shared_ptr<FileSystem>& fs,
                              const FileOptions& file_options) {
 
  // Create file reader.
  std::unique_ptr<RandomAccessFileReader> file_reader;
  s = RandomAccessFileReader::Create(fs, PathName(), file_options, &file_reader,
                                     nullptr);
        IOStatus RandomAccessFileReader::Create(
            const std::shared_ptr<FileSystem>& fs, const std::string& fname,
            const FileOptions& file_opts,
            std::unique_ptr<RandomAccessFileReader>* reader, IODebugContext* dbg) {
          std::unique_ptr<FSRandomAccessFile> file;
          IOStatus io_s = fs->NewRandomAccessFile(fname, file_opts, &file, dbg);
          if (io_s.ok()) {
            reader->reset(new RandomAccessFileReader(std::move(file), fname));
          }
          return io_s;
        }
```

blob_file.cc
```cpp
Status BlobFile::GetReader(Env* env, const FileOptions& file_options,
                           std::shared_ptr<RandomAccessFileReader>* reader,
                           bool* fresh_open) {
  s = env->GetFileSystem()->NewRandomAccessFile(PathName(), file_options,
        std::string BlobFile::PathName() const {
          return BlobFileName(path_to_dir_, file_number_);

```

I will first disable gc job to see if this issue can go away..
STill got too many open files error even I disable gc job.
Checking max_open_files
Saw other issues in rocksdb  which is similar to what I faced.

I now try to fix this issue by setting max_open_files to 4096

Still got this error after setting it to 8192. But log show that only 1024 is allowed to 
be opened.

Tried starting another process to verify that 
it's not a system issue.
No problem with another process.

maximum open files limit is 1024 .
Now I am trying to increase it.
1036 files opened by db_bench.
Is this the root cause?
I update nofile limit according to what gpt4 says.
But I don't think it works.
Set it to 512 to solve this issue for now.
```bash
 lsof -p 4027143 | head -n100
db_bench 4027143   zt   83r      REG  259,2     3300866  90569382 /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_gc_1.0_0.8/006739.sst
db_bench 4027143   zt   84r      REG  259,2     3752693  87530196 /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_gc_1.0_0.8/006830.sst
db_bench 4027143   zt   85r      REG  259,2     4147120  87532938 /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_gc_1.0_0.8/006636.sst
db_bench 4027143   zt   86r      REG  259,2     1705275  56256617 /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_gc_1.0_0.8/005383.sst
db_bench 4027143   zt   87r      REG  259,2     3301938  59986187 /mnt/nvme1n1/mlsm/test_blob_with_model_with_orig_gc/with_gc_1.0_0.8/004713.sst
```
Hope this can work.



Certainly! To increase the maximum number of open files (file descriptors) in Linux, you can follow these steps:

1. **Check Current Limits**:
   First, let's find out your current limit for open files. Open a terminal and run the following command:
   ```
   $ ulimit -n
   ```
   The output will show the current soft limit (maximum number of open files allowed per session).

2. **View System-Wide File Descriptor Limit**:
   To see the system-wide maximum number of opened file descriptors, use this command:
   ```
   $ cat /proc/sys/fs/file-max
   ```
   The value displayed represents the total number of files that a user can have opened per login session. It may vary depending on your system.

3. **Set a New Global Limit**:
   To increase the limit, you can edit the kernel directive `fs.file-max`. Use the `sysctl` utility as the root user:
   ```
   # sysctl -w fs.file-max=500000
   ```
   This command sets the open file limit to 500,000. Note that this change will only remain active until the next reboot.

4. **Make the Change Permanent**:
   To make the change permanent, edit the `/etc/sysctl.conf` file:
   ```
   # vi /etc/sysctl.conf
   ```
   Add the following line:
   ```
   fs.file-max=500000
   ```
   Adjust the number as needed. Save the file and exit.

5. **Verify the Change**:
   Confirm the updated value with:
   ```
   $ cat /proc/sys/fs/file-max
   ```

6. **User-Level File Limits**:
   If you want to set limits per user basis, edit the `/etc/security/limits.conf` file. This allows you to manage system resources by limiting users/groups at different levels.

Remember that users will need to **log out and log in again** for the changes to take effect. If you want to apply the limit immediately, use:
```
# sysctl -p
```


Looks like it works after setting max_open_files to 512.
[STatus: Done]

[Todo]
Why creating dataset res is not 0?
num_features_ is zero
It's because that I don't pass num_features to db.
[Status: Done]


[Todo]
Do statistics about lifetime of keys whose write count <= 1 
And then we can put keys that are initially inserted to that lifetime.
I don't think I have time to do that .
[Status: Not started]

I need to tell a good story.
I will say that my lsm is lifetime aware..

I need to get experiment results before I start writing paper.
Otherwise it's non sense to write a paper without any good results.

My point to do get for each flush is that lsm-tree is realatively small.
So doing get is relatively cheap.


[Todo]
ycsb top 5% keys take 50% write count.
```bash
➜  ycsb_a git:(main) ✗ python3 write_access_count.py
95% write count: 2451824
top 5% write count: 2548231 
```

[]

[Todo]
Put initial keys to long lifetime bucket.
Waster limited amount of space but get much better wamp.
If we assume 95% of keys are written less than twice.
[Status: Ongoing]


[Todo]
Adjust level size to aligh to xinqi's test.
There seems no difference int terms of compaction trigger size of for each level.
Is this because of we had too much data in value part ? 
Previous run is too slow.
Too many files are accumulated at L0 and L1

std rocksdb
```
➜  with_gc_0.2_0.4 grep LevelMaxLevelSize LOG | wc -l
200
```

mlsm:
```
(base) ➜  with_gc_1.0_0.8 grep LevelMaxLevelSize LOG | wc -l
2102
```
No need to adjust anything because we use the same config for all levels.
[Status: Abandoned]


[Todo]
Generate run worklaod with 50M of update in ycsb.
Actually is 25M writes.
[Status: Done]

[Todo]
Create a new branch without model prediction 
Need to verify that model works.
Actaully we can not put all keys to long lifetime without model. 
Then we don't know where to put keys during gc.
[Status: Ongoing]



[Todo]
Test stratagy
First load, set enable_gc_to false.
And then we enable gc and start write workoad from run worklaod data.
Or we could just start just writing run workload data.

dedicated gc with model default max
```bash
 Sum    779/0    2.11 GB   0.0     42.7    11.9     22.3      36.3     14.0       0.4   2.4      9.5     13.3   4579.23           4465.49      2945    1.555    126M    13M       8.5      23.2
(base) ➜  with_gc_1.0_0.8 grep 'start gc' LOG | wc -l
951

(base) ➜  test_blob_with_model_with_dedicated_gc grep 'Deleted file.*blob.*blob' 2024-03-12-15-55-
34-LOG-with-model-default-max-lifetime-bucket| wc -l
1404
```


dedicated gc with model default 0
```bash

 Sum    742/3    2.06 GB   0.0     76.8    12.1     23.1      37.3     14.1       0.7   3.5     11.2     12.9   7013.37           6883.84      3007    2.332    158M    17M      41.6      51.3

(base) ➜  test_blob_with_model_with_dedicated_gc grep 'start gc' 2024-03-12-15-55-34-LOG-with-model-default-max-lifetime-bucket  | wc -l
951

➜  test_blob_without_model_dedicated_gc grep 'Deleted file.*blob.*blob' 2024-03-12-15-54-57-LOG-with-model-default-0 | wc -l
1448
```
dedicated gcwithout model
Actaully I am currently running bench with model even though I set 
default lifetime bucket to 0.
Need to modify this.
I will first let this run finish and copy the log.
And then I will start another run without model.

Run another bench without model with default lifetime as max. failed because of lack of storage space.
Will do this later.

dedicated gc with model default:(max-1)
```
2024/03/12-21:30:34.322041 74197 [db/blob/blob_file_builder.cc:497] [default] [JOB 1536] Generated blob file #5486: 4754 total blobs, 6161184 total bytes, lifetime label: 2 creation timestamp: 12046877
2024/03/12-21:46:03.416467 74192 EVENT_LOG_v1 {"time_micros": 1710251163416456, "job": 2295, "event": "gc_started", "gc_files": [5486]}
```
[Status: Ongoing]


[Todo]
Problem: wamp in report.tst is lower than that in LOG 
when running with 0 default lifetime.
Need to figure out why.
```bash
    wamp=$( echo "( $sum_wgb + $flush_wgb ) / $cum_writes_gb" | bc -l | awk '{ printf "%.1f", $1 }' )
```
sum_wgb does not include write_blob_gb. So it's not correct. 
Need to check LOG to get true wamp.
[Status: Done]

[Todo]
Problem: blob offset map items are not deleted in the run. Becaseu 
we set default lifetime bucket index to high.
How can we solve this problem?
[Status: Not started]


[Todo]
Add training sample for valid keys during gc.
[Status: Not started]

[Todo]
Wait a little bit to aggregate more blob files
before start gc.
[Status: Not started]

[Todo]
Need to get read and write performance data.
Does rocksdb have this stattistics data?
```cpp
class StatisticsImpl : public Statistics {
 public:
  StatisticsImpl(std::shared_ptr<Statistics> stats);
  virtual ~StatisticsImpl();
  const char* Name() const override { return kClassName(); }
  static const char* kClassName() { return "BasicStatistics"; }

  virtual uint64_t getTickerCount(uint32_t ticker_type) const override;
  virtual void histogramData(uint32_t histogram_type,
                             HistogramData* const data) const override;
  std::string getHistogramString(uint32_t histogram_type) const override;

  virtual void setTickerCount(uint32_t ticker_type, uint64_t count) override;
  virtual uint64_t getAndResetTickerCount(uint32_t ticker_type) override;
  virtual void recordTick(uint32_t ticker_type, uint64_t count) override;
  // The function is implemented for now for backward compatibility reason.
 
```
I will just do it myself.
Check how kvbench written by yf implents read and write rate count
There is FinishiedOps in db_bench that can helps me get 
read and write rate count.
I think I should use that instead of writing code myself.
How to do that ?
```
  void FinishedOps(DBWithColumnFamilies* db_with_cfh, DB* db, int64_t num_ops,



```

```
  void Report(const Slice& name) {
```
Do see read and write performance data in log output .
I will copy this log t oexperimental results folder for future reference.
Cool.
Solve this problem without writing any code myself.

[Status: Done]

[Todo]
We can push key which are predicted wrong to data set and then train model.
Pay more attention to these keys.
[Status: Not started]

[Todo]
Change to use fast single infererence model function calling .
[Status: Done]


[Todo]
Test with 8M as default lifetime. 
gc invalid key ratio drops from 0.6 to 0.5 after setting default lifetime shorter.
Need to run more test and get more data to get a clear picture and decide 
which default lifetime should we set.
[Status: Ongoing]

[Todo]
Generate ycsb data with evenly write keys.
The goal is to beat hashkv.
[Status: Not started]


[Todo]
Add read cache to accelerate get during flush
Need to pass cache object to flush job.
Need to check how rocksdb use cache to get key.
```cpp
Status TableCache::Get(
    const ReadOptions& options,
    const InternalKeyComparator& internal_comparator,
    const FileMetaData& file_meta, const Slice& k, GetContext* get_context,
    const std::shared_ptr<const SliceTransform>& prefix_extractor,
    HistogramImpl* file_read_hist, bool skip_filters, int level,
    size_t max_file_size_for_l0_meta_pin) {

    if (t == nullptr) {
      s = FindTable(options, file_options_, internal_comparator, file_meta,
                    &handle, prefix_extractor,
                    options.read_tier == kBlockCacheTier /* no_io */,
                    true /* record_read_stats */, file_read_hist, skip_filters,
                    level, true /* prefetch_index_and_filter_in_cache */,
                    max_file_size_for_l0_meta_pin, file_meta.temperature);
 
      get_context->SetReplayLog(row_cache_entry);  // nullptr if no cache.
      s = t->Get(options, k, get_context, prefix_extractor.get(), skip_filters);

        Status BlockBasedTable::Get(const ReadOptions& read_options, const Slice& key,
                                    GetContext* get_context,
                                    const SliceTransform* prefix_extractor,
                                    bool skip_filters) {

      NewDataBlockIterator<DataBlockIter>(
          read_options, v.handle, &biter, BlockType::kData, get_context,
          &lookup_data_block_context, /*prefetch_buffer=*/nullptr,
          /*for_compaction=*/false, /*async_read=*/false, tmp_status);

 
```

```cpp
TBlockIter* BlockBasedTable::NewDataBlockIterator(
    const ReadOptions& ro, const BlockHandle& handle, TBlockIter* input_iter,
 
    s = RetrieveBlock(
        prefetch_buffer, ro, handle, dict, &block.As<IterBlocklike>(),
        get_context, lookup_context, for_compaction,
        /* use_cache */ true, /* wait_for_cache */ true, async_read);
 
        WithBlocklikeCheck<Status, TBlocklike> BlockBasedTable::RetrieveBlock(
            FilePrefetchBuffer* prefetch_buffer, const ReadOptions& ro,
            const BlockHandle& handle, const UncompressionDict& uncompression_dict,

           Status s;
          if (use_cache) {
            s = MaybeReadBlockAndLoadToCache(
                prefetch_buffer, ro, handle, uncompression_dict, wait_for_cache,
                for_compaction, out_parsed_block, get_context, lookup_context,
         
                BlockBasedTable::MaybeReadBlockAndLoadToCache(
                    FilePrefetchBuffer* prefetch_buffer, const ReadOptions& ro,
                    const BlockHandle& handle, const UncompressionDict& uncompression_dict,

                   BlockCacheInterface<TBlocklike> block_cache{
                      rep_->table_options.block_cache.get()};

 
                  bool is_cache_hit = false;
                  if (block_cache) {
                    // create key for block cache
                    key_data = GetCacheKey(rep_->base_cache_key, handle);
                    key = key_data.AsSlice();

                    if (!contents) {
                      s = GetDataBlockFromCache(key, block_cache, out_parsed_block, wait,
                                                get_context);
                      // Value could still be null at this point, so check the cache handle
                      // and update the read pattern for prefetching
                      if (out_parsed_block->GetValue() || out_parsed_block->GetCacheHandle()) {
                        // TODO(haoyu): Differentiate cache hit on uncompressed block cache and
                        // compressed block cache.
                        is_cache_hit = true;

                    if (BlockCacheTraceHelper::IsGetOrMultiGetOnDataBlock(
                            trace_block_type, lookup_context->caller)) {
                      // Defer logging the access to Get() and MultiGet() to trace additional
                      // information, e.g., referenced_key_exist_in_block.

                      // Make a copy of the block key here since it will be logged later.
                      lookup_context->FillLookupContext(
                          is_cache_hit, no_insert, trace_block_type,
                          /*block_size=*/usage, /*block_key=*/key.ToString(), nkeys);

                  void FillLookupContext(bool _is_cache_hit, bool _no_insert,
                                         TraceType _block_type, uint64_t _block_size,
                                         const std::string& _block_key,
                                         uint64_t _num_keys_in_block) {
                    is_cache_hit = _is_cache_hit;
                    no_insert = _no_insert;
                    block_type = _block_type;
                    block_size = _block_size;
                    block_key = _block_key;
                    num_keys_in_block = _num_keys_in_block;
                  }

```
I think I can just use Version->Get() to get key .
And set no_filled_cache, no read_blob.
This should work.
Does internal iterator use block cache? If so I won't do any change
I think internal iterator already use block cache.
Still need to use version to get key. Need to make sure 
we get key meta feautre from keys in L0 as well.
The worst case is that lots of keys are flush to level0
so later keys won't have any key meta. Thus we lose 
past write information of this key.
```
    readoptionscopy.snapshot = db->GetSnapshot();
      s = db->Get(readoptionscopy, key_slices[i], value);
    db->ReleaseSnapshot(readoptionscopy.snapshot);
```

```
  ReadOptions roptions;
  if (take_snapshot) {
    roptions.snapshot = db->GetSnapshot();
    db->GetDBOptions().env->SleepForMicroseconds(
        static_cast<int>(delay_ms * 1000));
  }


```
Don't wthink I can Use GetSnapshot for each get.
Maybe I can use internal iterator and update read options to not fill
 cache.. Maybe fill cache is ok as well because lsm-tree is small enough
to hold many blocks.
This should work.
CAn we get sequence number of imm memtable??

```cpp
  // kMaxSequenceNumber will be returned.
  SequenceNumber GetEarliestSequenceNumber() {
    return earliest_seqno_.load(std::memory_order_relaxed);
  }


```

We can use this earliest_seqno_ to create internal iterator
from rocksdb.. 
didnt' change many things after checking code .
Update kMaxSequenceNumber to smallest_seqno.


Add adaptive_readahead = true to flush job .
Maybe we can se t this true.

Don't think I need to do 
[Status: Done]

[Todo]
Check titan gc param.
```cpp
std::unique_ptr<BlobGC> BasicBlobGCPicker::PickBlobGC(
    BlobStorage*
     blob_storage) {
  Status s;
  std::vector<std::shared_ptr<BlobFileMeta>> blob_files;

  uint64_t batch_size = 0;
  uint64_t estimate_output_size = 0;
  bool stop_picking = false;
  bool maybe_continue_next_time = false;
  uint64_t next_gc_size = 0;
  bool in_fallback = cf_options_.blob_run_mode == TitanBlobRunMode::kFallback;

  for (auto& gc_score : blob_storage->gc_score()) {
    if (gc_score.score < cf_options_.blob_file_discardable_ratio) {
      break;
 
```
[Status: Done]

[Todo]
Add mispredicted keys to training sample.
[Status: Ongoing]


[Todo]
Change regression task to multi-class classification task.
[Status: Not started]


[Todo]
Count number of invalid keys that has past_distance_idx > 0;
[Status: Ongoing]

[todo]
Put keys to bucket based on  their sequence  number.
The result is pretty good.
We get better space amp after we update next lifetime label cauculation
 for each key during gc.
[Status: Ongoing]


OH fuck, there is no drastic improvment when intrducing model 
Another big failure I met during this project.
Why does the model not work?


[Todo]
Offiline evaluation of ycsb dataset with model prediction..
[Status: Not started]

[Todo]
YCSB a theata in which keys have at least 5 times writes. 
[Status: Not started]

[Todo]
Fix lightgbm single row predict error if possible.
Switch from predict row fast to normal predict.
Didn't get this erro after doing the swithc
[Status: Done]


[Todo]

Get ycsb 
https://github.com/lixiao-c/YCSB-generator?tab=readme-ov-file

Test diff skewness of ycsb
```bash
0.8
➜  ycsb_a git:(dev_use_cache_in_flush_and_gc) ✗ python3 write_access_count.py
95% write count: 1505402
top 5% write count: 3495118



0.99
➜  ycsb_a git:(dev_use_cache_in_flush_and_gc) ✗ python3 write_access_count.py
95% write count: 2451824
top 5% write count: 2548231


0.5
➜  ycsb_a git:(dev_use_cache_in_flush_and_gc) ✗ python3 write_access_count.py
95% write count: 4052923
top 5% write count: 947597


0.2 
➜  ycsb_a git:(dev_use_cache_in_flush_and_gc) ✗ python3 write_access_count.py
95% write count: 4338559
top 5% write count: 661961
```

Still got 60% of keys with one time write. 


Not much imporvement after introducing model for 50M uniform ycsb workload.
with model
wamp : 3.1
total size: 15.1

without model:
wamp: 2.7
total size: 16.1


Now try ycsba with zipfian 0.2 50M
Still not so good.
Data: 0.2 zipfian ycsb a 50M , 25 M read 25M write
with model: 
wamp : 3.1
blob total size: 9.3GB
gc input blob: 32252788, gc output blobs: 19916996, gc dropped blobs: 12335792, gc invalid key ratio: 0.382
Microseconds per write:
Count: 25001560 Average: 119.4157  StdDev: 282.22
Microseconds per read:
Count: 24998440 Average: 38.2915  StdDev: 5427.23
Uptime(secs): 3945.0 total, 0.1 interval


without model:
wamp : 2.6
blob total size: 12.4 GB
gc input blob: 18650475, gc output blobs: 4172342, gc dropped blobs: 14478133, gc invalid key ratio: 0.776
Microseconds per write:
Count: 25001560 Average: 135.7310  StdDev: 91.24
Microseconds per read:
Count: 24998440 Average: 32.2354  StdDev: 5031.52
Key drop rate :0.146118721
Uptime(secs): 4201.3 total, 1.0 interval

with multi-class model(relwithdebinfo mode):
wamp: 2.6
blob total size:  11.5 GB
gc input blob: 19509959, gc output blobs: 4852176, gc dropped blobs: 14657783, gc invalid key ratio: 0.751
Microseconds per write:
Count: 25001560 Average: 119.1656  StdDev: 175.06
Microseconds per read:
Count: 24998440 Average: 34.2789  StdDev: 5504.66
Key drop rate:  0.145631068 
Uptime(secs): 3838.3 total, 55.5 interval


with multi-class model(debug mode)
wamp : 2.6
blob total size: 11.6 GB
gc input blob: 20225822, gc output blobs: 5127970, gc dropped blobs: 15097852, gc invalid key ratio: 0.746
Microseconds per write:
Count: 25001560 Average: 161.3566  StdDev: 882.57
Microseconds per read:
Count: 24998440 Average: 115.1719  StdDev: 12958.56

[Status: Done]


Maybe we can test latest distribution of ycsb

[Todo]
Update existing model instead of training new model each time.
mlsm_scripts/mixgraph/generate_features.py
Model is not working after call BoosterMerge. I don't know why
Maybe I wrongly understand the BoosterMerge semantics.

It might be because of that the first model always gives zero prediction 
which affects the overall  model prediction.

[Status: Ongoing]

[Todo]
Remove compaction infer data file to get faster write?
[Status: Not started]

[Todo]
Need to mention in paper that we use both update frequence and age information 
for each keys.
According to Midas paper.
[Status: Not started]

[Todo]
Try catboost?
[Status: Not started]

[Todo]
Why do we need to get lifetime distribution ?
I want to put keys with zero writes or one writes to a good default lifetime bucket
so that their invalidated time is close to this default lifetime bucket time.
[Status: Not started]

[Todo]
Adaptive lifetime bucket creation. 
Search paper.
[Status: Not started]

[Todo]
https://github.com/Microsoft/LightGBM/issues/1157
```
[LightGBM] [Warning] No further splits with positive gain, best gain: -inf
it means:

1. the num_leaves is too large, you can set it to a smaller value
2. the min_data is too large
3. your data is hard to fit
```
[Status: Not started]



[Todo]
Figure out why there is no other lifetime bucket index prediction geven by 
model even the model is already trained.

Reduce short lifetime bucket
Default 0 -> 4M
no model:
wamp: 239.8 / 75.148 = 3.19
total size: 10.5 GB
gc input blob: 71973523, gc output blobs: 31137071, gc dropped blobs: 40836452, gc invalid key ratio: 0.567

with model:
wamp: 237.5 / 75 = 3.1666
total size: 11.2 GB
gc input blob: 69644186, gc output blobs: 29469641, gc dropped blobs: 40174545, gc invalid key ratio: 0.577




Default 2 -> 16M
no model:
wamp : 205.6/75.148= 2.73593442
total size: 20.7 GB
gc input blob: 34828472, gc output blobs: 2475138, gc dropped blobs: 32353334, gc invalid key ratio: 0.929

with model:
wamp:  205.2 / 75 = 2.736 
total size: 20.6 GB
gc input blob: 34981003, gc output blobs: 2625372, gc dropped blobs: 32355631, gc invalid key ratio: 0.925
Why is there so little gc input ?
Steps to take:
1. add model is available info in log
2. Add model prediction numeric value to training and infer log.
3. Add default index count and model prediction count to log.

Seems that I found that bug.
It's because of default predicted result value comparison.
My code does not set predicted value to 0 if possbile.

Print confusion matrix to get more detail prediction performance in training  . 

Model tends to predict lifetime:1 to lifetime:0 according to log but it's only a 
small proportion.
[Status: Done]

Lightgbm python code example for multi class classification.
https://www.geeksforgeeks.org/multiclass-classification-using-lightgbm/


Learn more about shared_ptr creation and deletion in multiple thread.s
https://stackoverflow.com/questions/14482830/stdshared-ptr-thread-safety
It's not safe to update shared_ptr in multiple threads.


[Todo]
Plot blob total size and wamp with different default lifetime bucket. 
[Status: Not started]

[Todo]
Add prediction to second time write of keys.
lrb does this as well.
So let's try. 

Add keys with 2 writes to training sample.

Add default nullptr to booster_ in flush job to 
avoid invalid pointer accessing .

Add read write mutext to protect booster that shared_ptr poitns to 
when doing booster update..

At the start of the flush job there is no model.
But after  a while there is model why is that ?
Is this because of swap? So I think I need to use lock to protect model call as well.
Remove swap and change to use assign.

Turns out it's not shared_ptr issue. It's that I did not add index of first distance 
to indices.
Hope this can fix the problem.
It gets really complicated when there are many data structures that 
depends on each other.

[Status: Ongoing]


[Todo]
Pay more attention to minority labelled keys.
auc mu has class_weight parameters.
I can try this.
Anyway, I need to finish introduction and background asap.
Background is not so important.
What's important is introduction and method part.
Really don't know if I can finish paper by that time.
```
metric:
auc_mu
multi_logloss
```
[Status: Ongoing]


[Todo]
Only one gc job is running at a time. Should we increase the number of gc job?
[Status: Not started]


[Todo]
Consider delay model prediction to level0-level1 compaction

Consider use this option to delay model prediction to level0-level1 compaction.
```
  int blob_file_starting_level = 0;
```

compaction_job.cc
```cpp
std::vector<std::unique_ptr<BlobFileBuilder>> blob_file_builders(db_options_.num_classification  );
  std::vector<BlobFileBuilder*> blob_file_builders_raw( db_options_.num_classification, nullptr);
  bool enable_blob_file_builder = mutable_cf_options->enable_blob_files &&
       sub_compact->compaction->output_level() >=
           mutable_cf_options->blob_file_starting_level;
    // mutable_cf_options.enable_blob_files &&
 
```
builder.cc
```cpp
Status BuildTable(
    const std::string& dbname, VersionSet* versions,
 
    const size_t lifetime_bucket_size = LifetimeSequence.size();
    std::vector<std::unique_ptr<BlobFileBuilder>> blob_file_builders(lifetime_bucket_size);
    std::vector<BlobFileBuilder*> blob_file_builders_raw( lifetime_bucket_size, nullptr);
    bool enable_blob_file_builder = mutable_cf_options.enable_blob_files &&
         tboptions.level_at_creation >=
             mutable_cf_options.blob_file_starting_level && blob_file_additions;


```

```cpp
if [ "$enable_blob_files" == "1" ]; then
  target_file_size_base=${TARGET_FILE_SIZE_BASE:-$((32 * write_buffer_size / value_size))}
else
  target_file_size_base=${TARGET_FILE_SIZE_BASE:-$write_buffer_size}
fi

max_bytes_for_level_base=${MAX_BYTES_FOR_LEVEL_BASE:-$((8 * target_file_size_base))}

  // Target file size for compaction.
  // target_file_size_base is per-file size for level-1.
  // Target file size for level L can be calculated by
  // target_file_size_base * (target_file_size_multiplier ^ (L-1))
  // For example, if target_file_size_base is 2MB and
  // target_file_size_multiplier is 10, then each file on level-1 will
  // be 2MB, and each file on level 2 will be 20MB,
  // and each file on level-3 will be 200MB.
  //
  // Default: 64MB.
  //
  // Dynamically changeable through SetOptions() API
  uint64_t target_file_size_base = 64 * 1048576;

  // Control maximum total data size for a level.
  // max_bytes_for_level_base is the max total for level-1.
  // Maximum number of bytes for level L can be calculated as
  // (max_bytes_for_level_base) * (max_bytes_for_level_multiplier ^ (L-1))
  // For example, if max_bytes_for_level_base is 200MB, and if
  // max_bytes_for_level_multiplier is 10, total data size for level-1
  // will be 200MB, total file size for level-2 will be 2GB,
  // and total file size for level-3 will be 20GB.
  //
  // Default: 256MB.
  //
  // Dynamically changeable through SetOptions() API
  uint64_t max_bytes_for_level_base = 256 * 1048576;

```

Difference between target_file_size_base and max_bytes_for_level_base.

Decide to set blob_file_starting_level to 1 and delay model prediction to l1
Seems that I don't need to do any source code modification other 
than set blob_file_starting_level=1
Oh, actually I need to do seom source code change.
I need to pass db_iter to compaction iter in compaction job.
Remove assert(false) in ExtractLargeValueIfNeededImpl()
because now we can have large value in level0.

Need to remove paranoid_file_checks to avoid put value in block cache.
Hope this can give use better write performance.
[Status: Ongoing]

































