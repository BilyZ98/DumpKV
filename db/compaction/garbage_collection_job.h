

#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "db/blob/blob_file_completion_callback.h"
#include "db/column_family.h"
// #include "db/compaction/compaction_iterator.h"
// #include "db/compaction/compaction_outputs.h"
#include "db/flush_scheduler.h"
#include "db/internal_stats.h"
#include "db/job_context.h"
#include "db/log_writer.h"
#include "db/memtable_list.h"
#include "db/range_del_aggregator.h"
#include "db/seqno_to_time_mapping.h"
#include "db/version_edit.h"
#include "db/write_controller.h"
#include "db/write_thread.h"
#include "logging/event_logger.h"
#include "options/cf_options.h"
#include "options/db_options.h"
#include "port/port.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/compaction_job_stats.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "db/blockingconcurrentqueue.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/transaction_log.h"
#include "table/scoped_arena_iterator.h"
#include "util/autovector.h"
#include "util/stop_watch.h"
#include "util/thread_local.h"

namespace ROCKSDB_NAMESPACE {

class Arena;
class CompactionState;
class ErrorHandler;
class MemTable;
class SnapshotChecker;
class SystemClock;
class TableCache;
class Version;
class VersionEdit;
class VersionSet;

class SubcompactionState;

class GarbageCollectionOutput{
public:
  GarbageCollectionOutput(GarbageCollection* gc);

  std::vector<BlobFileAddition>* blob_file_additions() {
    return &blob_file_additions_;
  }
private:
  GarbageCollection* gc_;
  std::vector<BlobFileAddition> blob_file_additions_;

};

class GarbageCollectionJob {
public:
  GarbageCollectionJob(int job_id, GarbageCollection* gc,
                       const ImmutableDBOptions& db_options,
                      const MutableDBOptions& mutable_db_options,
                      const FileOptions& file_options, VersionSet* versions,
                      const std::atomic<bool>* shutting_down,
                       LogBuffer* log_buffer,
                       FSDirectory* db_directory,
                       FSDirectory* output_directory,
                       FSDirectory* blob_directory,
                      Statistics* stats,
                       InstrumentedMutex* db_mutex,
                       ErrorHandler* error_handler,
                      JobContext* job_context,
                      EventLogger* event_logger,
                      const std::string& dbname, 
                       CompactionJobStats* compaction_job_stats,
                       Env::Priority thread_pri,
                       DBImpl* db,
                      moodycamel::BlockingConcurrentQueue<std::vector<double>>* training_data_queue,
                       std::shared_ptr<BoosterHandle> booster_handle
                       );


  virtual ~GarbageCollectionJob();
  GarbageCollectionJob(const GarbageCollectionJob&) = delete;
  GarbageCollectionJob& operator=(const GarbageCollectionJob&) = delete;

  // REQUIRED: mutex held
  // Prepare for the compaction by setting up boundaries for each subcompaction
  void Prepare();
  Status Run(InternalIterator* iter);

  // REQUIRED: mutex held
  // Add compaction input/output to the current version
  Status Install(const MutableCFOptions& mutable_cf_options);

  // Return the IO status
  IOStatus io_status() const { return io_status_; }

protected:
  Status ProcessGarbageCollection(InternalIterator* iter);
  Status WriteInferDataToFile(const std::vector<double>& data, double label) ;
  uint64_t GetNewLifetimeIndex(InternalIterator* iter);

  bool GetKeyMeta(const Slice& key, const Slice& value, std::vector<double>& key_meta);

  uint64_t GetNextLifetimeIndex(uint64_t cur_seq, uint64_t key_seq );

  void UpdateBlobStats(); 
  void UpdateGarbageCollectionStats();
  void LogGarbageCollection();

  virtual void RecordCompactionIOStats();
  void CleanupGarbageCollection();

  GarbageCollection* gc_;
  InternalStats::CompactionStatsFull gc_stats_;
  const ImmutableDBOptions& db_options_;
  const MutableDBOptions mutable_db_options_copy_;
  LogBuffer* log_buffer_;
  FSDirectory* output_directory_;
  Statistics* stats_;
  // Is this compaction creating a file in the bottom most level?
   IOStatus io_status_;

  CompactionJobStats* compaction_job_stats_;


  // Status InstallCompactionResults(const MutableCFOptions& mutable_cf_options);
  Status InstallGarbageCollectionResults(const MutableCFOptions& mutable_cf_options);
  // Status OpenCompactionOutputFile(SubcompactionState* sub_compact,
  //                                 CompactionOutputs& outputs);
  void UpdateGarbageCollectionJobStats(
      const InternalStats::CompactionStats& stats) const;
  // void UpdateCompactionJobStats(
  //     const InternalStats::CompactionStats& stats) const;
  void RecordDroppedKeys(const CompactionIterationStats& c_iter_stats,
                         CompactionJobStats* compaction_job_stats = nullptr);


  uint32_t job_id_;
  // DBImpl state
  const std::string& dbname_;
  const std::string db_id_;
  const std::string db_session_id_;
  const FileOptions file_options_;

  // std::vector<BlobFileAddition> blob_file_additions_;
  Env* env_;
  std::shared_ptr<IOTracer> io_tracer_;
  FileSystemPtr fs_;
  // env_option optimized for compaction table reads
  FileOptions file_options_for_read_;
  VersionSet* versions_;
  const std::atomic<bool>* shutting_down_;
  // const std::atomic<bool>& manual_compaction_canceled_;
  FSDirectory* db_directory_;
  FSDirectory* blob_output_directory_;
  InstrumentedMutex* db_mutex_;
  ErrorHandler* db_error_handler_;
   JobContext* job_context_;

  std::shared_ptr<Cache> table_cache_;

  EventLogger* event_logger_;
  Env::Priority thread_pri_;
  DBImpl* db_;

  Env::WriteLifeTimeHint write_hint_;
  UnorderedMap<uint64_t, UnorderedMap<std::string, std::string>*> blob_offset_map_;

  GarbageCollectionOutput gc_output_;

  std::unordered_map<uint64_t, uint64_t> blob_file_map_;

  moodycamel::BlockingConcurrentQueue<std::vector<double>>* training_data_queue_;
  uint64_t  num_features_ = 0;
  uint64_t default_lifetime_idx_=0;
  std::shared_ptr<BoosterHandle> booster_handle_ = nullptr;

  std::vector<uint64_t> lifetime_keys_count_;

  std::unique_ptr<WritableFile> infer_data_file_ = nullptr;;
};

}
