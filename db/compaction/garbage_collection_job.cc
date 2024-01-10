#include "db/compaction/compaction_job.h"
#include "db/compaction/garbage_collection_job.h"

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "db/blob/blob_counting_iterator.h"
#include "db/blob/blob_file_addition.h"
#include "db/blob/blob_file_builder.h"
#include "db/builder.h"
#include "db/compaction/clipping_iterator.h"
#include "db/compaction/compaction_state.h"
#include "db/db_impl/db_impl.h"
#include "db/dbformat.h"
#include "db/error_handler.h"
#include "db/event_helpers.h"
#include "db/history_trimming_iterator.h"
#include "db/log_writer.h"
#include "db/merge_helper.h"
#include "db/range_del_aggregator.h"
#include "db/version_edit.h"
#include "db/version_set.h"
#include "file/filename.h"
#include "file/read_write_util.h"
#include "file/sst_file_manager_impl.h"
#include "file/writable_file_writer.h"
#include "logging/log_buffer.h"
#include "logging/logging.h"
#include "monitoring/iostats_context_imp.h"
#include "monitoring/thread_status_util.h"
#include "options/configurable_helper.h"
#include "options/options_helper.h"
#include "port/port.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/options.h"
#include "rocksdb/statistics.h"
#include "rocksdb/status.h"
#include "rocksdb/table.h"
#include "rocksdb/utilities/options_type.h"
#include "table/merging_iterator.h"
#include "table/table_builder.h"
#include "table/unique_id_impl.h"
#include "test_util/sync_point.h"
#include "util/stop_watch.h"

namespace ROCKSDB_NAMESPACE {


GarbageCollectionJob::GarbageCollectionJob(
    int job_id,
    GarbageCollection* gc,
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
    const std::string& dbname, 
     CompactionJobStats* compaction_job_stats)
    : gc_(gc),
      db_options_(db_options),
      mutable_db_options_copy_(mutable_db_options),
      log_buffer_(log_buffer),
      output_directory_(output_directory),
      stats_(stats),
      compaction_job_stats_(compaction_job_stats),
      job_id_(job_id),
      dbname_(dbname),
      env_(db_options_.env),
      fs_(db_options.fs, nullptr),
      versions_(versions),
      shutting_down_(shutting_down),

      db_directory_(db_directory),
      db_mutex_(db_mutex),
      db_error_handler_(error_handler),
      job_context_(job_context) 
      {

  assert(log_buffer_ != nullptr);
  // assert(compaction_job_stats_ != nullptr);

}

GarbageCollectionJob::~GarbageCollectionJob() {

}

void GarbageCollectionJob::Prepare() {

}
Status GarbageCollectionJob::Run  () {
  LogGarbageCollection();
  return Status::OK();

}

// REQUIRED: mutex held
// Add compaction input/output to the current version
Status GarbageCollectionJob::Install(const MutableCFOptions& mutable_cf_options) {
  return Status::OK();

}

void GarbageCollectionJob::UpdateCompactionStats() {

}
void GarbageCollectionJob::LogGarbageCollection() {
  ROCKS_LOG_INFO(
        db_options_.info_log, "start gc job");


}
void GarbageCollectionJob::RecordCompactionIOStats() {

}
void GarbageCollectionJob::CleanupCompaction() {

}



// Status InstallCompactionResults(const MutableCFOptions& mutable_cf_options);
Status GarbageCollectionJob::InstallGarbageCollectionResults(const MutableCFOptions& mutable_cf_options) {

  return Status::OK();
}
// Status OpenCompactionOutputFile(SubcompactionState* sub_compact,
//                                 CompactionOutputs& outputs);
void GarbageCollectionJob::UpdateGarbageCollectionJobStats(
    const InternalStats::CompactionStats& stats) const {

}
// void UpdateCompactionJobStats(
//     const InternalStats::CompactionStats& stats) const;
void GarbageCollectionJob::RecordDroppedKeys(const CompactionIterationStats& c_iter_stats,
                       CompactionJobStats* compaction_job_stats  ) {

}



}
