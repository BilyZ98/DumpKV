
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
