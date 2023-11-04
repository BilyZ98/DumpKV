
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
