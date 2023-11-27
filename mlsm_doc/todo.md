
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



[Todo]
Try adding more features to improve classification accuracy and recall rate.

[Status: Not started]

[Todo] log blob files and sst files for each garbage collection.
May need to change gc implementation as well. Current gc implementation 
is not good for our idea.
[Status: Not started]
