#include "db/compaction/garbage_collection.h"
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
#include "db/blob/blob_file_addition.h"
#include "db/blob/blob_index.h"
#include "db/blob/blob_log_format.h"
#include "db/blob/blob_log_sequential_reader.h"


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
    EventLogger* event_logger,
    const std::string& dbname, 
     CompactionJobStats* compaction_job_stats,
    Env::Priority thread_pri,
    DBImpl* db)
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
      job_context_(job_context),
      event_logger_(event_logger),
      thread_pri_(thread_pri),
      db_(db)
      {

  assert(log_buffer_ != nullptr);
  // assert(compaction_job_stats_ != nullptr);

}

GarbageCollectionJob::~GarbageCollectionJob() {

}

void GarbageCollectionJob::Prepare() {

}
Status GarbageCollectionJob::Run(InternalIterator* iter) {
  assert(iter != nullptr);

  log_buffer_->FlushBufferToLog();
  LogGarbageCollection();
  Status s =ProcessGarbageCollection(iter);
  return s ;
}

// REQUIRED: mutex held
// Add compaction input/output to the current version
Status GarbageCollectionJob::Install(const MutableCFOptions& mutable_cf_options) {
  assert(gc_);
  db_mutex_->AssertHeld();

  Status status;
  ColumnFamilyData* cfd = gc_->column_family_data();
  assert(cfd);
 
  cfd->internal_stats()->AddGCStats(thread_pri_, gc_stats_);
  status = InstallGarbageCollectionResults(mutable_cf_options);
  assert(status.ok());

  auto stream = event_logger_->LogToBuffer(log_buffer_, 8192);
  stream << "job" << job_id_ << "event"
    << "gc_finished";


  auto vstorage = cfd->current()->storage_info();
  const auto& blob_files = vstorage->GetBlobFiles();
  const auto& lifetime_blob_files = vstorage->GetLifetimeBlobFiles();
  if (!blob_files.empty()) {
    assert(blob_files.front());
    stream << "blob_file_head" << blob_files.front()->GetBlobFileNumber();

    assert(blob_files.back());
    stream << "blob_file_tail" << blob_files.back()->GetBlobFileNumber();

    stream << "blob_files";
    stream.StartArray();
    for(const auto& blob_file:blob_files) {
      stream << blob_file->GetBlobFileNumber() ;
    }
    stream.EndArray();

    for(size_t i=0; i < lifetime_blob_files.size(); ++i) {
      std::string key = "lifetime_blob_" + std::to_string(i);
      stream << key; 
      stream.StartArray();
      for(const auto& blob_file:lifetime_blob_files[i]) {
        stream << blob_file->GetBlobFileNumber() ;
      }
      stream.EndArray();
    }
    
  }


  CleanupGarbageCollection();

  return status;

}

Status GarbageCollectionJob::ProcessGarbageCollection(InternalIterator* iter) {

  std::ofstream fout;
  // Status st = env_->CreateDirIfMissing(dbname_ + "/job_ratio/");
  // assert(st.ok());
  // fout.open(dbname_ + "/job_ratio/" + std::to_string(job_id_)+ ".txt", std::ios::out);

  bool isout=true;

  // std::unique_ptr<Env> mock_env_;
  FileSystem* fs_in;
  SystemClock* clock_;
  FileOptions file_options_in;
  
  // 得到blob_files文件
  ColumnFamilyData* cfd_in = gc_->column_family_data();
  assert(cfd_in);
  VersionStorageInfo* vstorage = cfd_in->current()->storage_info();
  VersionStorageInfo::BlobFiles blob_files = vstorage->GetBlobFiles();
  // std::sort(blob_files.begin(),  blob_files.end(), myfunction);
  fs_in = env_->GetFileSystem().get();
  clock_ = env_->GetSystemClock().get();

  Status s ;
  // 统计垃圾回收前垃圾的分布
  uint64_t valid_key_num = 0;
  uint64_t invalid_key_num = 0;
  uint64_t valid_value_size = 0;
  uint64_t invalid_value_size = 0;
  uint64_t valid_key_size = 0;
  uint64_t invalid_key_size = 0;
  uint64_t total_blob_size = 0;
  const uint64_t write_batch_cnt = 1024;
  auto write_options = WriteOptions();
  WriteBatch wb;

  for (auto blob_file: *(gc_->inputs())){
    // 读取blob file

    uint64_t blob_file_num = blob_file->GetBlobFileNumber();
    std::string blob_file_path = BlobFileName(dbname_, blob_file->GetBlobFileNumber());;

    std::unique_ptr<FSRandomAccessFile> file;
    constexpr IODebugContext* dbg = nullptr;
    s =fs_in->NewRandomAccessFile(blob_file_path, file_options_in, &file, dbg);
  // If the entry read from LSM Tree and vlog file point to the same vlog file and offset,
  // insert them back into the DB.
  // so we only need to compare value of internal iterator in lsm-tree which
  // has file id and file offset.. 
  // how can we get that ?

    if (!s.ok()){
        return s;
    }
    std::unique_ptr<RandomAccessFileReader> file_reader(
        new RandomAccessFileReader(std::move(file), blob_file_path, clock_));

    constexpr Statistics* statistics = nullptr;
    BlobLogSequentialReader blob_log_reader(std::move(file_reader), clock_,
                                            statistics);

    BlobLogHeader header;
// std::cout<<"blob_file_path: "<<blob_file_path<<std::endl;
     s = blob_log_reader.ReadHeader(&header);
      if (!s.ok()){
          return s;
      }

    uint64_t blob_offset = 0;

    auto cf_handle = db_-> DefaultColumnFamily();


    while (true) {
      BlobLogRecord record;

      rocksdb::Status s2 = blob_log_reader.ReadRecord(&record, BlobLogSequentialReader::kReadHeaderKeyBlob, &blob_offset);
      if (!s2.ok()){
        break;
      }
      // Slice record_internal_key = record.key;
      Slice blob_user_key(record.key);
      blob_user_key.remove_suffix(8);
      Slice blob_value(record.value);

      // user_key.remove_suffix(8);
      LookupKey lkey(blob_user_key, kMaxSequenceNumber);
      // InternalIterator* internalIterator = iter->get();
      iter->Seek(lkey.internal_key());
      s = iter->status();
      if (!s.ok()){
      return s;
      }
      if(iter->Valid() ) {

        Slice find_internal_key = iter->key();
        ParsedInternalKey parsed_ikey;
        s = ParseInternalKey(find_internal_key, &parsed_ikey, false);
        if(!s.ok()) {
          return s;
        }
        
        if(parsed_ikey.user_key.compare(blob_user_key)==0 &&  parsed_ikey.type == rocksdb::kTypeBlobIndex) {
          uint64_t cur_blob_offset = blob_offset;
          uint64_t cur_blob_file_num = blob_file_num; 
          BlobIndex blob_index;
          s = blob_index.DecodeFrom(iter->value());
          if (!s.ok()) {
            return s;
          }
  
          uint64_t iter_blob_file_num = blob_index.file_number();
          uint64_t iter_blob_offset = blob_index.offset();

          if((cur_blob_file_num == iter_blob_file_num) && (cur_blob_offset == iter_blob_offset)) {


            // s = db_->Put(write_options, cf_handle, blob_user_key,  blob_value);
            // s = wb.PutGC(cf_handle, record.key, blob_value);
            if(!s.ok()) {
              if(!s.IsShutdownInProgress()){
                assert(false);
              }
              // assert(false);
              return s;
            }
            if(wb.Count() >= write_batch_cnt) {
              // s = db_->Write(write_options, &wb);
              if(!s.ok()) {
                if(!s.IsShutdownInProgress()){
                  assert(false);
                }
                return s;
              }
              wb.Clear();
            }

            valid_key_num++;
            valid_value_size+=record.value_size;
            valid_key_size+=record.key_size;
          } else {
            invalid_key_num++;
            invalid_value_size+=record.value_size;
            invalid_key_size+=record.key_size;
          }

          total_blob_size+=record.record_size();

        }
      }

     }
   
    // if (isout){
    //   fout<<"job_id: "<<job_id_<<std::endl;
    //   isout=false;
    // }
    // fout << "BlobFileNumber: "      << blob_file->GetBlobFileNumber() << " ";
    // fout << "valid_key_num: "       << valid_key_num << " ";
    // fout << "invalid_key_num: "     << invalid_key_num << " ";
    // fout << "ratio: "               << float(invalid_key_num)/(valid_key_num+invalid_key_num) << " ";
    // fout << "valid_value_size: "    << valid_value_size << " ";
    // fout << "invalid_value_size: "  << invalid_value_size << " ";
    // fout << "ratio: "               << float(invalid_value_size)/(valid_value_size+invalid_value_size) << " ";
    // fout << "GarbageBlobCount: "    << blob_file->GetGarbageBlobCount() << " ";
    // fout << "GarbageBlobBytes: "    << blob_file->GetGarbageBlobBytes() << " ";
    // fout << std::endl;
  

      BlobLogFooter footer;
      blob_log_reader.ReadFooter(&footer);
    
  }
  if(wb.Count() > 0) {
    // s = db_->Write(write_options, &wb);
    if(!s.ok()) {
      if(!s.IsShutdownInProgress()){
        assert(false);
      }
      return s;
    }
  }
  gc_stats_.stats.num_input_records = valid_key_num + invalid_key_num;
  gc_stats_.stats.bytes_read_blob = total_blob_size;
  gc_stats_.stats.num_output_records = valid_key_num;
  gc_stats_.stats.bytes_written = valid_value_size + valid_key_size;
  gc_stats_.stats.num_dropped_records = invalid_key_num;


  
  UpdateGarbageCollectionStats();
  auto stream = event_logger_->LogToBuffer(log_buffer_, 8192);
  stream << "job" << job_id_ << "valid_key_num" << valid_key_num 
    << "invalid_key_num" << invalid_key_num 
    << "valid_value_size" << valid_value_size
    << "invalid_value_size" << invalid_value_size
    << "invalid_key_ratio" << float(invalid_key_num)/(valid_key_num+invalid_key_num);

  LogFlush(db_options_.info_log); 


  return s;
}

void GarbageCollectionJob::UpdateGarbageCollectionStats() {

}
void GarbageCollectionJob::LogGarbageCollection() {
  ROCKS_LOG_INFO(
        db_options_.info_log, "start gc job");

    auto stream = event_logger_->Log();
    stream << "job" << job_id_ << "event"
           << "gc_started";

    stream << "gc_files"  ;
    stream.StartArray();
    for(const auto& blob_file : *(gc_->inputs())) {
      stream <<  blob_file->GetBlobFileNumber();
    }
    stream.EndArray();


  log_buffer_->FlushBufferToLog();

}
void GarbageCollectionJob::RecordCompactionIOStats() {

}
void GarbageCollectionJob::CleanupGarbageCollection() {
  // delete gc_;
  // gc_ = nullptr;

}



// Status InstallCompactionResults(const MutableCFOptions& mutable_cf_options);
Status GarbageCollectionJob::InstallGarbageCollectionResults(const MutableCFOptions& mutable_cf_options) {
  VersionEdit* const edit = gc_->edit();
  gc_->AddInputDeletions(edit);
  return versions_->LogAndApply(gc_->column_family_data(), 
                                mutable_cf_options, edit,
                                db_mutex_, db_directory_);
  // return Status::OK();
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
