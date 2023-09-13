
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







