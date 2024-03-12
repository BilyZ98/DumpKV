#include "db/compaction/garbage_collection.h"
#include "db/compaction/garbage_collection_job.h"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
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

GarbageCollectionOutput::GarbageCollectionOutput(GarbageCollection* gc) 
: gc_(gc) {
  assert(gc_ != nullptr);
}

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
      db_(db),
      write_hint_(Env::WLTH_NOT_SET),
      gc_output_(gc)
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

  const uint64_t start_micros = db_options_.clock->NowMicros();
  log_buffer_->FlushBufferToLog();
  LogGarbageCollection();
  Status s =ProcessGarbageCollection(iter);

  gc_stats_.SetMicros(db_options_.clock->NowMicros() - start_micros);
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
  double invalid_key_ratio = double(cfd->internal_stats()->GetGCStats().gc_dropped_blobs) / double(cfd->internal_stats()->GetGCStats().gc_input_blobs);
  ROCKS_LOG_INFO(db_options_.info_log, "gc input blob: %lu, gc output blobs: %lu, gc dropped blobs: %lu, gc invalid key ratio: %.3f", 
                 cfd->internal_stats()->GetGCStats().gc_input_blobs,
                 cfd->internal_stats()->GetGCStats().gc_output_blobs,
                 cfd->internal_stats()->GetGCStats().gc_dropped_blobs,
                 invalid_key_ratio);
  status = InstallGarbageCollectionResults(mutable_cf_options);
  assert(status.ok());

  const auto & stats = gc_stats_.stats;
  auto stream = event_logger_->LogToBuffer(log_buffer_, 8192);
  stream << "job" << job_id_ << "event"
    << "gc_finished"
    << "gc_time_micros" << stats.micros
    << "gc_time_cpu_micros" << stats.cpu_micros
    << "total_output_size" << stats.bytes_written_blob;




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

  uint64_t prev_cpu_micros = db_options_.clock->CPUMicros();
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
  // uint64_t valid_key_size = 0;
  // uint64_t invalid_key_size = 0;
  uint64_t total_blob_size = 0;
  // auto write_options = WriteOptions();
  WriteBatch wb;

  std::vector<std::string> blob_file_paths;
  std::vector<std::unique_ptr<BlobFileBuilder>> blob_file_builders(LifetimeSequence.size() );
  std::vector<BlobFileBuilder*> blob_file_builders_raw( LifetimeSequence.size(), nullptr);
    // auto vstorage = cfd_->current()->storage_info();

    // auto blob_file_meta = vstorage->FastGetBlobFileMetaData(blob_index.file_number());

  for(size_t i =0; i < blob_file_builders.size(); i++){
    // uint64_t timestamp = env_->NowMicros();
    uint64_t now_seq = versions_->LastSequence();
    blob_file_builders[i] = std::unique_ptr<BlobFileBuilder>(
    new BlobFileBuilder(
      versions_, fs_.get(),
      gc_->immutable_options(),
      gc_->mutable_cf_options(), &file_options_, db_id_, db_session_id_,
      job_id_, cfd_in->GetID(), cfd_in->GetName(), Env::IOPriority::IO_LOW,
      write_hint_, io_tracer_, nullptr,
      BlobFileCreationReason::kCompaction, &blob_file_paths,
      gc_output_.blob_file_additions(),
        i, now_seq));

      blob_file_builders_raw[i] = blob_file_builders[i].get();
  }

  std::string blob_index_str;
  std::string prev_blob_index_str;

  std::vector<uint64_t> oldest_blob_file_creation_time(LifetimeSequence.size(), std::numeric_limits<uint64_t>::max());

  for (auto blob_file: *(gc_->inputs())){
    // 读取blob file

    uint64_t blob_file_num = blob_file->GetBlobFileNumber();
    std::string blob_file_path = BlobFileName(dbname_, blob_file->GetBlobFileNumber());;
    uint64_t lifetime_label = blob_file->GetLifetimeLabel();
    uint64_t next_lifetime_label = std::min(lifetime_label + 1, LifetimeSequence.size() - 1);
    oldest_blob_file_creation_time[next_lifetime_label] = std::min(oldest_blob_file_creation_time[next_lifetime_label], blob_file->GetCreationTimestamp());

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
    // blob file builder might create multiple new blob files.
    // default blob file size limit is 256MB.
    // what should I do?
    // Create a new function in blob file builder 
    // and it doesn't close blob file until close blob 
    // file function is called.
    // Don't have any restriction on blob file size for now .
    // Let's complte the logic and then we make optimization.


    while (true) {
      BlobLogRecord record;

      rocksdb::Status s2 = blob_log_reader.ReadRecord(&record, BlobLogSequentialReader::kReadHeaderKeyBlob, &blob_offset);
      if (!s2.ok()){
        break;
      }

      ParsedInternalKey parsed_blob_key;
      s = ParseInternalKey(record.key, &parsed_blob_key, false);
      Slice blob_value(record.value);

      LookupKey lkey(parsed_blob_key.user_key, kMaxSequenceNumber);
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
        
        if(parsed_ikey.user_key.compare(parsed_blob_key.user_key)==0 &&
          parsed_ikey.sequence == parsed_blob_key.sequence &&
          parsed_ikey.type == rocksdb::kTypeBlobIndex) {
          uint64_t cur_blob_offset = blob_offset;
          uint64_t cur_blob_file_num = blob_file_num; 


          blob_index_str.clear();
          prev_blob_index_str.clear();
          BlobIndex::EncodeBlob(&prev_blob_index_str, blob_file_num, blob_offset, record.value_size,
                               CompressionType::kNoCompression);
          s = blob_file_builders[next_lifetime_label]->AddWithoutBlobFileClose(record.key, record.value, &blob_index_str);
          assert(s.ok());
          BlobIndex new_blob_index;
          new_blob_index.DecodeFrom(blob_index_str);
          uint64_t new_blob_file_num = blob_file_builders[next_lifetime_label]->GetCurBlobFileNum();
          if(blob_offset_map_.find(new_blob_file_num) == blob_offset_map_.end()) {
            blob_offset_map_[new_blob_file_num] = new UnorderedMap<std::string, std::string>();
          }
          auto res = blob_offset_map_[new_blob_file_num]->emplace(prev_blob_index_str, blob_index_str);
          assert(res.second);

          if(!s.ok()) {
            if(!s.IsShutdownInProgress()){
              assert(false);
            }
            return s;
          }
          valid_key_num++;
          valid_value_size+=record.value_size;
          // valid_key_size+=record.key_size;
        } else {
          invalid_key_num++;
          invalid_value_size+=record.value_size;
          // invalid_key_size+=record.key_size;
        }
      } else {
        invalid_key_num++;
        invalid_value_size+=record.value_size;
        // invalid_key_size+=record.key_size;
      }

      total_blob_size+=record.record_size();
    }
   

      
    BlobLogFooter footer;
    blob_log_reader.ReadFooter(&footer);

    uint64_t cur_new_blob_file = blob_file_builders[next_lifetime_label]->GetCurBlobFileNum();
    auto res = blob_file_map_.emplace(blob_file_num, cur_new_blob_file);
    assert(res.second);
    s = blob_file_builders[next_lifetime_label]->CloseBlobFileIfNeeded() ;
    //
    // s = blob_file_builders[next_lifetime_label]->CloseBlobFileIfNeeded(oldest_blob_file_creation_time[next_lifetime_label]) ;
    // if(!blob_file_builders[next_lifetime_label]->IsBlobFileOpen()) {
    //   oldest_blob_file_creation_time[next_lifetime_label] = std::numeric_limits<uint64_t>::max();
    // }

    assert(s.ok());
    
  }
  
  for(size_t i =0; i < blob_file_builders.size(); i++){
    s = blob_file_builders[i]->Finish();
    if (!s.ok()) {
      assert(false);
      return s;
    }
  }
  gc_stats_.stats.bytes_read_blob = total_blob_size;
  // gc_stats_.stats.bytes_written = valid_value_size + valid_key_size;
  UpdateBlobStats();
  gc_stats_.stats.num_output_files_blob = gc_output_.blob_file_additions()->size();
  gc_stats_.stats.num_input_records = valid_key_num + invalid_key_num;
  gc_stats_.stats.num_dropped_records = invalid_key_num;
  gc_stats_.stats.num_output_records = valid_key_num;
  gc_stats_.stats.gc_input_blobs = valid_key_num + invalid_key_num;
  gc_stats_.stats.gc_output_blobs = valid_key_num;
  gc_stats_.stats.gc_dropped_blobs = invalid_key_num;

 gc_stats_.stats.cpu_micros =
      db_options_.clock->CPUMicros() - prev_cpu_micros;
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
void GarbageCollectionJob::UpdateBlobStats() {
  gc_stats_.stats.num_output_files_blob = gc_output_.blob_file_additions()->size();
  for (const auto& blob :*gc_output_.blob_file_additions()) {
    gc_stats_.stats.bytes_written_blob += blob.GetTotalBlobBytes();
  }
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
  std::unordered_map<uint64_t, BlobGarbageMeter::BlobStats> blob_total_garbage;

  for (const auto& blob : *gc_output_.blob_file_additions()) {
    edit->AddBlobFile(blob);
  }
  assert(!blob_file_map_.empty());
  edit->AddBlobFileMap(&blob_file_map_);
  assert(!blob_offset_map_.empty());
  edit->AddBlobOffsetMap(&blob_offset_map_);

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
