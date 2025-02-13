//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "db/compaction/compaction_iterator.h"

#include <cstdint>
#include <iterator>
#include <limits>
#include <algorithm>
#include <string>
#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "rocksdb/key_meta.h"
#include "rocksdb/utilities/ldb_cmd.h"

#include "db/blob/blob_fetcher.h"
#include "db/blob/blob_file_builder.h"
#include "db/blob/blob_index.h"
#include "db/blob/prefetch_buffer_collection.h"
#include "db/snapshot_checker.h"
#include "db/db_impl/db_impl.h"
#include "logging/logging.h"
#include "port/likely.h"
#include "rocksdb/listener.h"
#include "rocksdb/compaction_trace_writer.h"
#include "table/internal_iterator.h"
#include "test_util/sync_point.h"
#include "util/coding.h"

namespace ROCKSDB_NAMESPACE {
const  std::unordered_map<uint64_t, uint64_t> LifetimeLabelToSecMap ={
  {0, 1000000},
  {1, 3000000}
};

#define M 1000000
// const std::vector<uint64_t> LifetimeSequence = { 1 * M, 2 * M, 4*M, 8 * M, 16 * M };
std::vector<uint64_t> LifetimeSequence = {  4*M, 8 * M, 4 * M };
// const  std::unordered_map<uint64_t, uint64_t> LifetimeLabelToSecMap ={
//   {0, 1},
//   {1, 10},
//   {2, 100},
//   {3, 1000}
// };

// const std::vector<uint64_t> LifetimeSecs = { 1, 10 , 100, 1000};
const std::vector<uint64_t> LifetimeSecs = { 281, 1000};


CompactionIterator::CompactionIterator(
    InternalIterator* input, const Comparator* cmp, MergeHelper* merge_helper,
    SequenceNumber last_sequence, std::vector<SequenceNumber>* snapshots,
    SequenceNumber earliest_write_conflict_snapshot,
    SequenceNumber job_snapshot, const SnapshotChecker* snapshot_checker,
    Env* env, bool report_detailed_time, bool expect_valid_internal_key,
    CompactionRangeDelAggregator* range_del_agg,
    std::vector<BlobFileBuilder*> blob_file_builders, bool allow_data_in_errors,
    bool enforce_single_del_contracts,
    const std::atomic<bool>& manual_compaction_canceled,
    std::shared_ptr<BoosterHandle> booster_handle,
    std::shared_ptr<FastConfigHandle> fast_config_handle,
    const autovector<MemTable*>* mems,
    ColumnFamilyData* cfd,
    const VersionSet* version_set,
    uint64_t num_classification,
    uint64_t num_features,
    uint64_t default_lifetime_idx,
    const Compaction* compaction, const CompactionFilter* compaction_filter,
    const std::atomic<bool>* shutting_down,
    const std::shared_ptr<Logger> info_log,
    const std::string* full_history_ts_low,
    const SequenceNumber preserve_time_min_seqno,
    const SequenceNumber preclude_last_level_min_seqno)
     :CompactionIterator(
          input, cmp, merge_helper, last_sequence, snapshots,
          earliest_write_conflict_snapshot, job_snapshot, snapshot_checker, env,
          report_detailed_time, expect_valid_internal_key, range_del_agg,
          nullptr, allow_data_in_errors, enforce_single_del_contracts,
          manual_compaction_canceled,
          std::unique_ptr<CompactionProxy>(
              compaction ? new RealCompaction(compaction) : nullptr),
          compaction_filter, shutting_down, info_log, full_history_ts_low,
          preserve_time_min_seqno, preclude_last_level_min_seqno){
  cfd_ = cfd;
  mems_ = mems;
  fast_config_handle_ = fast_config_handle;
  booster_handle_ = booster_handle;
  num_classification_ = num_classification;
  num_features_ = num_features;
  default_lifetime_idx_ = default_lifetime_idx;
  version_set_ = version_set;
  lifetime_keys_count_.resize(blob_file_builders.size());
  for (size_t i = 0; i < lifetime_keys_count_.size(); i++) {
    lifetime_keys_count_[i] = 0;
  }
  if(booster_handle_){
    ROCKS_LOG_INFO(info_log_, "Model is available");
  } else {
    ROCKS_LOG_INFO(info_log_, "Model is not available");
  }

  const EnvOptions soptions;
  std::string infer_data_file_path = version_set_->GetDBName() + "/compaction_infer_data.txt" + std::to_string(env->NowMicros()) ;
  // Status s= env_->NewWritableFile(infer_data_file_path, &train_data_file_, soptions);
  // assert(s.ok());

  // is this ok?
  lifetime_blob_file_builders_ = std::move(blob_file_builders);

}
CompactionIterator::CompactionIterator(
    InternalIterator* input, const Comparator* cmp, MergeHelper* merge_helper,
    SequenceNumber last_sequence, std::vector<SequenceNumber>* snapshots,
    SequenceNumber earliest_write_conflict_snapshot,
    SequenceNumber job_snapshot, const SnapshotChecker* snapshot_checker,
    Env* env, bool report_detailed_time, bool expect_valid_internal_key,
    CompactionRangeDelAggregator* range_del_agg,
    BlobFileBuilder* blob_file_builder, bool allow_data_in_errors,
    bool enforce_single_del_contracts,
    const std::atomic<bool>& manual_compaction_canceled,
    const Compaction* compaction, const CompactionFilter* compaction_filter,
    const std::atomic<bool>* shutting_down,
    const std::shared_ptr<Logger> info_log,
    const std::string* full_history_ts_low,
    const SequenceNumber preserve_time_min_seqno,
    const SequenceNumber preclude_last_level_min_seqno)
    : CompactionIterator(
          input, cmp, merge_helper, last_sequence, snapshots,
          earliest_write_conflict_snapshot, job_snapshot, snapshot_checker, env,
          report_detailed_time, expect_valid_internal_key, range_del_agg,
          blob_file_builder, allow_data_in_errors, enforce_single_del_contracts,
          manual_compaction_canceled,
          std::unique_ptr<CompactionProxy>(
              compaction ? new RealCompaction(compaction) : nullptr),
          compaction_filter, shutting_down, info_log, full_history_ts_low,
          preserve_time_min_seqno, preclude_last_level_min_seqno) {}

CompactionIterator::CompactionIterator(
    InternalIterator* input, const Comparator* cmp, MergeHelper* merge_helper,
    SequenceNumber last_sequence, std::vector<SequenceNumber>* snapshots,
    SequenceNumber earliest_write_conflict_snapshot,
    SequenceNumber job_snapshot, const SnapshotChecker* snapshot_checker,
    Env* env, bool report_detailed_time, bool expect_valid_internal_key,
    CompactionRangeDelAggregator* range_del_agg,
    std::vector<BlobFileBuilder*> blob_file_builders, bool allow_data_in_errors,
    bool enforce_single_del_contracts,
    const std::atomic<bool>& manual_compaction_canceled,
    std::shared_ptr<BoosterHandle> booster_handle,
    std::shared_ptr<FastConfigHandle> fast_config_handle,
    ColumnFamilyData* cfd,
    uint64_t num_features,
    uint64_t default_lifetime_idx,
    const Compaction* compaction, const CompactionFilter* compaction_filter,
    const std::atomic<bool>* shutting_down,
    const std::shared_ptr<Logger> info_log,
    const std::string* full_history_ts_low,
    const SequenceNumber preserve_time_min_seqno,
    const SequenceNumber preclude_last_level_min_seqno)
    : CompactionIterator(
          input, cmp, merge_helper, last_sequence, snapshots,
          earliest_write_conflict_snapshot, job_snapshot, snapshot_checker, env,
          report_detailed_time, expect_valid_internal_key, range_del_agg,
          nullptr, allow_data_in_errors, enforce_single_del_contracts,
          manual_compaction_canceled,
          std::unique_ptr<CompactionProxy>(
              compaction ? new RealCompaction(compaction) : nullptr),
          compaction_filter, shutting_down, info_log, full_history_ts_low,
          preserve_time_min_seqno, preclude_last_level_min_seqno) {
    booster_handle_ = booster_handle;
    fast_config_handle_ = fast_config_handle;
    cfd_ = cfd;
    default_lifetime_idx_ = default_lifetime_idx;
    // version_set_ = version_set;
    lifetime_keys_count_.resize(blob_file_builders.size());
    num_features_ = num_features;
    lifetime_blob_file_builders_ = std::move(blob_file_builders);

    }


CompactionIterator::CompactionIterator(
    InternalIterator* input, const Comparator* cmp, MergeHelper* merge_helper,
    SequenceNumber /*last_sequence*/, std::vector<SequenceNumber>* snapshots,
    SequenceNumber earliest_write_conflict_snapshot,
    SequenceNumber job_snapshot, const SnapshotChecker* snapshot_checker,
    Env* env, bool report_detailed_time, bool expect_valid_internal_key,
    CompactionRangeDelAggregator* range_del_agg,
    BlobFileBuilder* blob_file_builder, bool allow_data_in_errors,
    bool enforce_single_del_contracts,
    const std::atomic<bool>& manual_compaction_canceled,
    std::unique_ptr<CompactionProxy> compaction,
    const CompactionFilter* compaction_filter,
    const std::atomic<bool>* shutting_down,
    const std::shared_ptr<Logger> info_log,
    const std::string* full_history_ts_low,
    const SequenceNumber preserve_time_min_seqno,
    const SequenceNumber preclude_last_level_min_seqno)
    : 
      compaction_tracer_(nullptr),
    input_(input, cmp,
             !compaction || compaction->DoesInputReferenceBlobFiles()),
      cmp_(cmp),
      merge_helper_(merge_helper),
      snapshots_(snapshots),
      earliest_write_conflict_snapshot_(earliest_write_conflict_snapshot),
      job_snapshot_(job_snapshot),
      snapshot_checker_(snapshot_checker),
      env_(env),
      clock_(env_->GetSystemClock().get()),
      report_detailed_time_(report_detailed_time),
      expect_valid_internal_key_(expect_valid_internal_key),
      range_del_agg_(range_del_agg),
      blob_file_builder_(blob_file_builder),
      compaction_(std::move(compaction)),
      compaction_filter_(compaction_filter),
      shutting_down_(shutting_down),
      manual_compaction_canceled_(manual_compaction_canceled),
      bottommost_level_(!compaction_ ? false
                                     : compaction_->bottommost_level() &&
                                           !compaction_->allow_ingest_behind()),
      // snapshots_ cannot be nullptr, but we will assert later in the body of
      // the constructor.
      visible_at_tip_(snapshots_ ? snapshots_->empty() : false),
      earliest_snapshot_(!snapshots_ || snapshots_->empty()
                             ? kMaxSequenceNumber
                             : snapshots_->at(0)),
      info_log_(info_log),
      allow_data_in_errors_(allow_data_in_errors),
      enforce_single_del_contracts_(enforce_single_del_contracts),
      timestamp_size_(cmp_ ? cmp_->timestamp_size() : 0),
      full_history_ts_low_(full_history_ts_low),
      current_user_key_sequence_(0),
      current_user_key_snapshot_(0),
      merge_out_iter_(merge_helper_),
      blob_garbage_collection_cutoff_file_number_(
          ComputeBlobGarbageCollectionCutoffFileNumber(compaction_.get())),
      blob_fetcher_(CreateBlobFetcherIfNeeded(compaction_.get())),
      prefetch_buffers_(
          CreatePrefetchBufferCollectionIfNeeded(compaction_.get())),
      current_key_committed_(false),
      cmp_with_history_ts_low_(0),
      level_(compaction_ == nullptr ? 0 : compaction_->level()),
      preserve_time_min_seqno_(preserve_time_min_seqno),
      preclude_last_level_min_seqno_(preclude_last_level_min_seqno) {
  assert(snapshots_ != nullptr);
  assert(preserve_time_min_seqno_ <= preclude_last_level_min_seqno_);

  cfd_ = nullptr;
  if (compaction_ != nullptr) {
    level_ptrs_ = std::vector<size_t>(compaction_->number_levels(), 0);
  }
#ifndef NDEBUG
  // findEarliestVisibleSnapshot assumes this ordering.
  for (size_t i = 1; i < snapshots_->size(); ++i) {
    assert(snapshots_->at(i - 1) < snapshots_->at(i));
  }
  assert(timestamp_size_ == 0 || !full_history_ts_low_ ||
         timestamp_size_ == full_history_ts_low_->size());
#endif
  input_.SetPinnedItersMgr(&pinned_iters_mgr_);
  // The default `merge_until_status_` does not need to be checked since it is
  // overwritten as soon as `MergeUntil()` is called
  merge_until_status_.PermitUncheckedError();
  TEST_SYNC_POINT_CALLBACK("CompactionIterator:AfterInit", compaction_.get());
  ROCKS_LOG_INFO(info_log_,
                        "blob_age_cutoff_file_number (%" PRIu64 ") "
                        "enable_blob_garbage_collection (%"  PRId32 ") ",
                        blob_garbage_collection_cutoff_file_number_,
                 compaction ? (compaction->enable_blob_garbage_collection() ? 1 : 0): 0);

}

CompactionIterator::~CompactionIterator() {
  // input_ Iterator lifetime is longer than pinned_iters_mgr_ lifetime
  input_.SetPinnedItersMgr(nullptr);
  if(train_data_file_ != nullptr) {

    train_data_file_->Close();
  }

  ROCKS_LOG_INFO(info_log_, "Num value blob index update: %lu",
                 num_blob_nidex_updated_);
}

void CompactionIterator::ResetRecordCounts() {
  iter_stats_.num_record_drop_user = 0;
  iter_stats_.num_record_drop_hidden = 0;
  iter_stats_.num_record_drop_obsolete = 0;
  iter_stats_.num_record_drop_range_del = 0;
  iter_stats_.num_range_del_drop_obsolete = 0;
  iter_stats_.num_optimized_del_drop_obsolete = 0;
}

void CompactionIterator::SeekToFirst() {
  NextFromInput();
  PrepareOutput();
}

void CompactionIterator::SetBoosterMutex(std::shared_mutex* booster_mutex) {
  booster_mutex_ = booster_mutex;
}
void CompactionIterator::SetModelAndConfig(BoosterHandle booster_handle, FastConfigHandle fast_config_handle) {
  // booster_handle_ = booster_handle;
  // fast_config_handle_ = fast_config_handle;
}



void CompactionIterator::SetKeyMeta(const std::unordered_map<std::string, KeyMeta>* key_metas , std::mutex* key_meta_mutex) {
  key_metas_ = key_metas;
  key_metas_mutex_ = key_meta_mutex;
}

void CompactionIterator::SetDBInternalIterator(InternalIterator* db_internal_iter) {
  db_internal_iter_ = db_internal_iter;
}

void CompactionIterator::SetMemTables(const autovector<MemTable*>* mems) {

  mems_ = mems;
}
void CompactionIterator::SetCompactionTracer(std::shared_ptr<CompactionTracer> tracer) {
  compaction_tracer_ = tracer;
}

void CompactionIterator::SetFeatures(const  std::unordered_map<std::string, std::unordered_map<uint64_t, std::vector<double>>>* features)   {
  features_ = features;

}

void CompactionIterator::SetDBImpl(DBImpl* db_impl) {
  db_ = db_impl;
}

void CompactionIterator::SetGCBlobFiles(const autovector<uint64_t>* gc_blob_files) {
  for (auto file_number : *gc_blob_files) {
    gc_blob_files_.insert(file_number);
  }

}


Status CompactionIterator::PredictLifetimeLabel( const KeyFeatures &kcontext , uint64_t* lifeteim_label)  {
  // int predict_res =  LGBM_BoosterPredictForMatSingleRowFast(FastConfigHandle fastConfig_handle,
  //                                                              const void* data,
  //                                                              int64_t* out_len,
  //                                                              double* out_result);

  return Status::OK();



}

void CompactionIterator::Next() {
  // If there is a merge output, return it before continuing to process the
  // input.
  if(compaction_tracer_.get() == nullptr) {
    fprintf(stderr, "compaction_tracer_ is nullptr in compaction iterator\n");
    assert(false);
  }
  if (merge_out_iter_.Valid()) {
    assert(false);
    merge_out_iter_.Next();

    // Check if we returned all records of the merge output.
    if (merge_out_iter_.Valid()) {
      key_ = merge_out_iter_.key();
      value_ = merge_out_iter_.value();
      Status s = ParseInternalKey(key_, &ikey_, allow_data_in_errors_);
      // MergeUntil stops when it encounters a corrupt key and does not
      // include them in the result, so we expect the keys here to be valid.
      if (!s.ok()) {
        ROCKS_LOG_FATAL(
            info_log_, "Invalid ikey %s in compaction. %s",
            allow_data_in_errors_ ? key_.ToString(true).c_str() : "hidden",
            s.getState());
        assert(false);
      }

      // Keep current_key_ in sync.
      if (0 == timestamp_size_) {
        current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
      } else {
        Slice ts = ikey_.GetTimestamp(timestamp_size_);
        current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type, &ts);
      }
      key_ = current_key_.GetInternalKey();
      ikey_.user_key = current_key_.GetUserKey();
      validity_info_.SetValid(ValidContext::kMerge1);
    } else {
      if (merge_until_status_.IsMergeInProgress()) {
        // `Status::MergeInProgress()` tells us that the previous `MergeUntil()`
        // produced only merge operands. Those merge operands were accessed and
        // written out using `merge_out_iter_`. Since `merge_out_iter_` is
        // exhausted at this point, all merge operands have been written out.
        //
        // Still, there may be a base value (PUT, DELETE, SINGLEDEL, etc.) that
        // needs to be written out. Normally, `CompactionIterator` would skip it
        // on the basis that it has already output something in the same
        // snapshot stripe. To prevent this, we reset `has_current_user_key_` to
        // trick the future iteration from finding out the snapshot stripe is
        // unchanged.
        has_current_user_key_ = false;
      }
      // We consumed all pinned merge operands, release pinned iterators
      pinned_iters_mgr_.ReleasePinnedData();
      // MergeHelper moves the iterator to the first record after the merged
      // records, so even though we reached the end of the merge output, we do
      // not want to advance the iterator.
      NextFromInput();
    }
  } else {
    // Only advance the input iterator if there is no merge output and the
    // iterator is not already at the next record.
    if (!at_next_) {
      // printf("not at next\n");
      AdvanceInputIter();
    }
    NextFromInput();
  }

  if (Valid()) {
    // Record that we've outputted a record for the current key.
    has_outputted_key_ = true;
  }

  PrepareOutput();
}

bool CompactionIterator::InvokeFilterIfNeeded(bool* need_skip,
                                              Slice* skip_until) {
  // TODO: support compaction filter for wide-column entities
  // assert(!compaction_filter_);
  if (!compaction_filter_ ||
      (ikey_.type != kTypeValue && ikey_.type != kTypeBlobIndex)) {

    return true;
  }
  assert(false);
  // fprintf(stderr, "Error: InvokeFilterIfNeeded\n");
  bool error = false;
  // If the user has specified a compaction filter and the sequence
  // number is greater than any external snapshot, then invoke the
  // filter. If the return value of the compaction filter is true,
  // replace the entry with a deletion marker.
  CompactionFilter::Decision filter = CompactionFilter::Decision::kUndetermined;
  compaction_filter_value_.clear();
  compaction_filter_skip_until_.Clear();
  CompactionFilter::ValueType value_type =
      ikey_.type == kTypeValue ? CompactionFilter::ValueType::kValue
                               : CompactionFilter::ValueType::kBlobIndex;
  // Hack: pass internal key to BlobIndexCompactionFilter since it needs
  // to get sequence number.
  assert(compaction_filter_);
  Slice& filter_key =
      (ikey_.type == kTypeValue ||
       !compaction_filter_->IsStackedBlobDbInternalCompactionFilter())
          ? ikey_.user_key
          : key_;
  {
    StopWatchNano timer(clock_, report_detailed_time_);
    if (kTypeBlobIndex == ikey_.type) {
      BlobIndex blob_index;
      Status s = blob_index.DecodeFrom(value_);
      uint64_t blob_file_num = blob_index.file_number();
      if(gc_blob_files_.find(blob_file_num) == gc_blob_files_.end()) {
        return true;
      }
      filter = compaction_filter_->FilterBlobByKey(
          level_, filter_key, &compaction_filter_value_,
          compaction_filter_skip_until_.rep());
      if (CompactionFilter::Decision::kUndetermined == filter &&
          !compaction_filter_->IsStackedBlobDbInternalCompactionFilter()) {
        if (compaction_ == nullptr) {
          status_ =
              Status::Corruption("Unexpected blob index outside of compaction");
          validity_info_.Invalidate();
          return false;
        }

        TEST_SYNC_POINT_CALLBACK(
            "CompactionIterator::InvokeFilterIfNeeded::TamperWithBlobIndex",
            &value_);

        // For integrated BlobDB impl, CompactionIterator reads blob value.
        // For Stacked BlobDB impl, the corresponding CompactionFilter's
        // FilterV2 method should read the blob value.
        // BlobIndex blob_index;
        // Status s = blob_index.DecodeFrom(value_);
        if (!s.ok()) {
          status_ = s;
          validity_info_.Invalidate();
          return false;
        }

        FilePrefetchBuffer* prefetch_buffer =
            prefetch_buffers_ ? prefetch_buffers_->GetOrCreatePrefetchBuffer(
                                    blob_index.file_number())
                              : nullptr;

        uint64_t bytes_read = 0;

        assert(blob_fetcher_);

        s = blob_fetcher_->FetchBlob(ikey_.user_key, blob_index,
                                     prefetch_buffer, &blob_value_,
                                     &bytes_read);
        if (!s.ok()) {
          status_ = s;
          validity_info_.Invalidate();
          return false;
        }

        ++iter_stats_.num_blobs_read;
        iter_stats_.total_blob_bytes_read += bytes_read;

        value_type = CompactionFilter::ValueType::kValue;
      }
    }
    if (CompactionFilter::Decision::kUndetermined == filter) {
      filter = compaction_filter_->FilterV2(
          level_, filter_key, value_type,
          blob_value_.empty() ? value_ : blob_value_, &compaction_filter_value_,
          compaction_filter_skip_until_.rep());
    }
    iter_stats_.total_filter_time +=
        env_ != nullptr && report_detailed_time_ ? timer.ElapsedNanos() : 0;
  }

  if (CompactionFilter::Decision::kUndetermined == filter) {
    // Should not reach here, since FilterV2 should never return kUndetermined.
    status_ =
        Status::NotSupported("FilterV2() should never return kUndetermined");
    validity_info_.Invalidate();
    return false;
  }

  if (filter == CompactionFilter::Decision::kRemoveAndSkipUntil &&
      cmp_->Compare(*compaction_filter_skip_until_.rep(), ikey_.user_key) <=
          0) {
    // Can't skip to a key smaller than the current one.
    // Keep the key as per FilterV2 documentation.
    filter = CompactionFilter::Decision::kKeep;
  }

  if (filter == CompactionFilter::Decision::kRemove) {
    // convert the current key to a delete; key_ is pointing into
    // current_key_ at this point, so updating current_key_ updates key()
    ikey_.type = kTypeDeletion;
    current_key_.UpdateInternalKey(ikey_.sequence, kTypeDeletion);
    // no value associated with delete
    // value_.clear();
    iter_stats_.num_record_drop_user++;
  } else if (filter == CompactionFilter::Decision::kPurge) {
    // convert the current key to a single delete; key_ is pointing into
    // current_key_ at this point, so updating current_key_ updates key()
    ikey_.type = kTypeSingleDeletion;
    current_key_.UpdateInternalKey(ikey_.sequence, kTypeSingleDeletion);
    // no value associated with single delete
    value_.clear();
    iter_stats_.num_record_drop_user++;
  } else if (filter == CompactionFilter::Decision::kChangeValue) {
    if (ikey_.type == kTypeBlobIndex) {
      // value transfer from blob file to inlined data
      ikey_.type = kTypeValue;
      current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
    }
    value_ = compaction_filter_value_;
  } else if (filter == CompactionFilter::Decision::kRemoveAndSkipUntil) {
    *need_skip = true;
    compaction_filter_skip_until_.ConvertFromUserKey(kMaxSequenceNumber,
                                                     kValueTypeForSeek);
    *skip_until = compaction_filter_skip_until_.Encode();
  } else if (filter == CompactionFilter::Decision::kChangeBlobIndex) {
    // Only the StackableDB-based BlobDB impl's compaction filter should return
    // kChangeBlobIndex. Decision about rewriting blob and changing blob index
    // in the integrated BlobDB impl is made in subsequent call to
    // PrepareOutput() and its callees.
    if (!compaction_filter_->IsStackedBlobDbInternalCompactionFilter()) {
      status_ = Status::NotSupported(
          "Only stacked BlobDB's internal compaction filter can return "
          "kChangeBlobIndex.");
      validity_info_.Invalidate();
      return false;
    }
    if (ikey_.type == kTypeValue) {
      // value transfer from inlined data to blob file
      ikey_.type = kTypeBlobIndex;
      current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
    }
    value_ = compaction_filter_value_;
  } else if (filter == CompactionFilter::Decision::kIOError) {
    if (!compaction_filter_->IsStackedBlobDbInternalCompactionFilter()) {
      status_ = Status::NotSupported(
          "CompactionFilter for integrated BlobDB should not return kIOError");
      validity_info_.Invalidate();
      return false;
    }
    status_ = Status::IOError("Failed to access blob during compaction filter");
    error = true;
  }
  return !error;
}

void CompactionIterator::NextFromInput() {
  at_next_ = false;
  validity_info_.Invalidate();

  while (!Valid() && input_.Valid() && !IsPausingManualCompaction() &&
         !IsShuttingDown()) {
    key_ = input_.key();
    value_ = input_.value();
    blob_value_.Reset();
    iter_stats_.num_input_records++;

    Status pik_status = ParseInternalKey(key_, &ikey_, allow_data_in_errors_);
    if (!pik_status.ok()) {
      iter_stats_.num_input_corrupt_records++;

      // If `expect_valid_internal_key_` is false, return the corrupted key
      // and let the caller decide what to do with it.
      if (expect_valid_internal_key_) {
        status_ = pik_status;
        return;
      }
      key_ = current_key_.SetInternalKey(key_);
      has_current_user_key_ = false;
      current_user_key_sequence_ = kMaxSequenceNumber;
      current_user_key_snapshot_ = 0;
      validity_info_.SetValid(ValidContext::kParseKeyError);
      break;
    }
    TEST_SYNC_POINT_CALLBACK("CompactionIterator:ProcessKV", &ikey_);

    // Update input statistics
    if (ikey_.type == kTypeDeletion || ikey_.type == kTypeSingleDeletion ||
        ikey_.type == kTypeDeletionWithTimestamp) {
      iter_stats_.num_input_deletion_records++;
    }
    iter_stats_.total_input_raw_key_bytes += key_.size();
    iter_stats_.total_input_raw_value_bytes += value_.size();

    // If need_skip is true, we should seek the input iterator
    // to internal key skip_until and continue from there.
    bool need_skip = false;
    // Points either into compaction_filter_skip_until_ or into
    // merge_helper_->compaction_filter_skip_until_.
    Slice skip_until;

    bool user_key_equal_without_ts = false;
    int cmp_ts = 0;
    if (has_current_user_key_) {
      user_key_equal_without_ts =
          cmp_->EqualWithoutTimestamp(ikey_.user_key, current_user_key_);
      // if timestamp_size_ > 0, then curr_ts_ has been initialized by a
      // previous key.
      cmp_ts = timestamp_size_ ? cmp_->CompareTimestamp(
                                     ExtractTimestampFromUserKey(
                                         ikey_.user_key, timestamp_size_),
                                     curr_ts_)
                               : 0;
    }

    // Check whether the user key changed. After this if statement current_key_
    // is a copy of the current input key (maybe converted to a delete by the
    // compaction filter). ikey_.user_key is pointing to the copy.
    if (!has_current_user_key_ || !user_key_equal_without_ts || cmp_ts != 0) {
      // First occurrence of this user key
      // Copy key for output
      key_ = current_key_.SetInternalKey(key_, &ikey_);

      int prev_cmp_with_ts_low =
          !full_history_ts_low_ ? 0
          : curr_ts_.empty()
              ? 0
              : cmp_->CompareTimestamp(curr_ts_, *full_history_ts_low_);

      // If timestamp_size_ > 0, then copy from ikey_ to curr_ts_ for the use
      // in next iteration to compare with the timestamp of next key.
      UpdateTimestampAndCompareWithFullHistoryLow();

      // If
      // (1) !has_current_user_key_, OR
      // (2) timestamp is disabled, OR
      // (3) all history will be preserved, OR
      // (4) user key (excluding timestamp) is different from previous key, OR
      // (5) timestamp is NO older than *full_history_ts_low_, OR
      // (6) timestamp is the largest one older than full_history_ts_low_,
      // then current_user_key_ must be treated as a different user key.
      // This means, if a user key (excluding ts) is the same as the previous
      // user key, and its ts is older than *full_history_ts_low_, then we
      // consider this key for GC, e.g. it may be dropped if certain conditions
      // match.
      if (!has_current_user_key_ || !timestamp_size_ || !full_history_ts_low_ ||
          !user_key_equal_without_ts || cmp_with_history_ts_low_ >= 0 ||
          prev_cmp_with_ts_low >= 0) {
        // Initialize for future comparison for rule (A) and etc.
        current_user_key_sequence_ = kMaxSequenceNumber;
        current_user_key_snapshot_ = 0;
        has_current_user_key_ = true;
      }
      current_user_key_ = ikey_.user_key;

      has_outputted_key_ = false;

      last_key_seq_zeroed_ = false;

      current_key_committed_ = KeyCommitted(ikey_.sequence);

      // Apply the compaction filter to the first committed version of the user
      // key.
      if (current_key_committed_ &&
          !InvokeFilterIfNeeded(&need_skip, &skip_until)) {
        break;
      }
    } else {
      // Update the current key to reflect the new sequence number/type without
      // copying the user key.
      // TODO(rven): Compaction filter does not process keys in this path
      // Need to have the compaction filter process multiple versions
      // if we have versions on both sides of a snapshot
      current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
      key_ = current_key_.GetInternalKey();
      ikey_.user_key = current_key_.GetUserKey();

      // Note that newer version of a key is ordered before older versions. If a
      // newer version of a key is committed, so as the older version. No need
      // to query snapshot_checker_ in that case.
      if (UNLIKELY(!current_key_committed_)) {
        assert(snapshot_checker_ != nullptr);
        current_key_committed_ = KeyCommitted(ikey_.sequence);
        // Apply the compaction filter to the first committed version of the
        // user key.
        if (current_key_committed_ &&
            !InvokeFilterIfNeeded(&need_skip, &skip_until)) {
          break;
        }
      }
    }

    if (UNLIKELY(!current_key_committed_)) {
      assert(snapshot_checker_ != nullptr);
      validity_info_.SetValid(ValidContext::kCurrentKeyUncommitted);
      break;
    }

    // If there are no snapshots, then this kv affect visibility at tip.
    // Otherwise, search though all existing snapshots to find the earliest
    // snapshot that is affected by this kv.
    SequenceNumber last_sequence = current_user_key_sequence_;
    current_user_key_sequence_ = ikey_.sequence;
    SequenceNumber last_snapshot = current_user_key_snapshot_;
    SequenceNumber prev_snapshot = 0;  // 0 means no previous snapshot
    if(ikey_.sequence ==0 ){
      // fprintf(stdout, " ikey_.sequence ==0 \n");
      assert(false);

    }
    current_user_key_snapshot_ =
        visible_at_tip_
            ? earliest_snapshot_
            : findEarliestVisibleSnapshot(ikey_.sequence, &prev_snapshot);

    if (need_skip) {
      // This case is handled below.
    } else if (clear_and_output_next_key_) {
      // In the previous iteration we encountered a single delete that we could
      // not compact out.  We will keep this Put, but can drop it's data.
      // (See Optimization 3, below.)
      if (ikey_.type != kTypeValue && ikey_.type != kTypeBlobIndex &&
          ikey_.type != kTypeWideColumnEntity) {
        ROCKS_LOG_FATAL(info_log_, "Unexpected key %s for compaction output",
                        ikey_.DebugString(allow_data_in_errors_, true).c_str());
        assert(false);
      }
      if (current_user_key_snapshot_ < last_snapshot) {
        ROCKS_LOG_FATAL(info_log_,
                        "key %s, current_user_key_snapshot_ (%" PRIu64
                        ") < last_snapshot (%" PRIu64 ")",
                        ikey_.DebugString(allow_data_in_errors_, true).c_str(),
                        current_user_key_snapshot_, last_snapshot);
        assert(false);
      }

      if (ikey_.type == kTypeBlobIndex || ikey_.type == kTypeWideColumnEntity) {
        ikey_.type = kTypeValue;
        current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
      }

      value_.clear();
      validity_info_.SetValid(ValidContext::kKeepSDAndClearPut);
      clear_and_output_next_key_ = false;
    } else if (ikey_.type == kTypeSingleDeletion) {
      fprintf(stderr,"should not be here: single delete\n");
      assert(false);
      // We can compact out a SingleDelete if:
      // 1) We encounter the corresponding PUT -OR- we know that this key
      //    doesn't appear past this output level
      // =AND=
      // 2) We've already returned a record in this snapshot -OR-
      //    there are no earlier earliest_write_conflict_snapshot.
      //
      // A note about 2) above:
      // we try to determine whether there is any earlier write conflict
      // checking snapshot by calling DefinitelyInSnapshot() with seq and
      // earliest_write_conflict_snapshot as arguments. For write-prepared
      // and write-unprepared transactions, if earliest_write_conflict_snapshot
      // is evicted from WritePreparedTxnDB::commit_cache, then
      // DefinitelyInSnapshot(seq, earliest_write_conflict_snapshot) returns
      // false, even if the seq is actually visible within
      // earliest_write_conflict_snapshot. Consequently, CompactionIterator
      // may try to zero out its sequence number, thus hitting assertion error
      // in debug mode or cause incorrect DBIter return result.
      // We observe that earliest_write_conflict_snapshot >= earliest_snapshot,
      // and the seq zeroing logic depends on
      // DefinitelyInSnapshot(seq, earliest_snapshot). Therefore, if we cannot
      // determine whether seq is **definitely** in
      // earliest_write_conflict_snapshot, then we can additionally check if
      // seq is definitely in earliest_snapshot. If the latter holds, then the
      // former holds too.
      //
      // Rule 1 is needed for SingleDelete correctness.  Rule 2 is needed to
      // allow Transactions to do write-conflict checking (if we compacted away
      // all keys, then we wouldn't know that a write happened in this
      // snapshot).  If there is no earlier snapshot, then we know that there
      // are no active transactions that need to know about any writes.
      //
      // Optimization 3:
      // If we encounter a SingleDelete followed by a PUT and Rule 2 is NOT
      // true, then we must output a SingleDelete.  In this case, we will decide
      // to also output the PUT.  While we are compacting less by outputting the
      // PUT now, hopefully this will lead to better compaction in the future
      // when Rule 2 is later true (Ie, We are hoping we can later compact out
      // both the SingleDelete and the Put, while we couldn't if we only
      // outputted the SingleDelete now).
      // In this case, we can save space by removing the PUT's value as it will
      // never be read.
      //
      // Deletes and Merges are not supported on the same key that has a
      // SingleDelete as it is not possible to correctly do any partial
      // compaction of such a combination of operations.  The result of mixing
      // those operations for a given key is documented as being undefined.  So
      // we can choose how to handle such a combinations of operations.  We will
      // try to compact out as much as we can in these cases.
      // We will report counts on these anomalous cases.
      //
      // Note: If timestamp is enabled, then record will be eligible for
      // deletion, only if, along with above conditions (Rule 1 and Rule 2)
      // full_history_ts_low_ is specified and timestamp for that key is less
      // than *full_history_ts_low_. If it's not eligible for deletion, then we
      // will output the SingleDelete. For Optimization 3 also, if
      // full_history_ts_low_ is specified and timestamp for the key is less
      // than *full_history_ts_low_ then only optimization will be applied.

      // The easiest way to process a SingleDelete during iteration is to peek
      // ahead at the next key.
      const bool is_timestamp_eligible_for_gc =
          (timestamp_size_ == 0 ||
           (full_history_ts_low_ && cmp_with_history_ts_low_ < 0));

      ParsedInternalKey next_ikey;
      AdvanceInputIter();

      // Check whether the next key exists, is not corrupt, and is the same key
      // as the single delete.
      if (input_.Valid() &&
          ParseInternalKey(input_.key(), &next_ikey, allow_data_in_errors_)
              .ok() &&
          cmp_->EqualWithoutTimestamp(ikey_.user_key, next_ikey.user_key)) {
#ifndef NDEBUG
        const Compaction* c =
            compaction_ ? compaction_->real_compaction() : nullptr;
#endif
        TEST_SYNC_POINT_CALLBACK(
            "CompactionIterator::NextFromInput:SingleDelete:1",
            const_cast<Compaction*>(c));
        if (last_key_seq_zeroed_) {
          CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
          compaction_tracer_->WriteDropKey(record);
          ++iter_stats_.num_record_drop_hidden;
          ++iter_stats_.num_record_drop_obsolete;
          assert(bottommost_level_);
          AdvanceInputIter();
        } else if (prev_snapshot == 0 ||
                   DefinitelyNotInSnapshot(next_ikey.sequence, prev_snapshot)) {
          // Check whether the next key belongs to the same snapshot as the
          // SingleDelete.

          TEST_SYNC_POINT_CALLBACK(
              "CompactionIterator::NextFromInput:SingleDelete:2", nullptr);
          if (next_ikey.type == kTypeSingleDeletion) {
            // We encountered two SingleDeletes for same key in a row. This
            // could be due to unexpected user input. If write-(un)prepared
            // transaction is used, this could also be due to releasing an old
            // snapshot between a Put and its matching SingleDelete.
            // Skip the first SingleDelete and let the next iteration decide
            // how to handle the second SingleDelete.

            // First SingleDelete has been skipped since we already called
            // input_.Next().
            CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
            compaction_tracer_->WriteDropKey(record);
            ++iter_stats_.num_record_drop_obsolete;
            ++iter_stats_.num_single_del_mismatch;
          } else if (next_ikey.type == kTypeDeletion) {
            std::ostringstream oss;
            oss << "Found SD and type: " << static_cast<int>(next_ikey.type)
                << " on the same key, violating the contract "
                   "of SingleDelete. Check your application to make sure the "
                   "application does not mix SingleDelete and Delete for "
                   "the same key. If you are using "
                   "write-prepared/write-unprepared transactions, and use "
                   "SingleDelete to delete certain keys, then make sure "
                   "TransactionDBOptions::rollback_deletion_type_callback is "
                   "configured properly. Mixing SD and DEL can lead to "
                   "undefined behaviors";
            ++iter_stats_.num_record_drop_obsolete;
            ++iter_stats_.num_single_del_mismatch;
            if (enforce_single_del_contracts_) {
              ROCKS_LOG_ERROR(info_log_, "%s", oss.str().c_str());
              validity_info_.Invalidate();
              status_ = Status::Corruption(oss.str());
              return;
            }
            ROCKS_LOG_WARN(info_log_, "%s", oss.str().c_str());
          } else if (!is_timestamp_eligible_for_gc) {
            // We cannot drop the SingleDelete as timestamp is enabled, and
            // timestamp of this key is greater than or equal to
            // *full_history_ts_low_. We will output the SingleDelete.
            validity_info_.SetValid(ValidContext::kKeepTsHistory);
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
            CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
            compaction_tracer_->WriteDropKey(record);
            ++iter_stats_.num_record_drop_hidden;
            ++iter_stats_.num_record_drop_obsolete;
            // Already called input_.Next() once.  Call it a second time to
            // skip past the second key.
            AdvanceInputIter();
          } else {
            // Found a matching value, but we cannot drop both keys since
            // there is an earlier snapshot and we need to leave behind a record
            // to know that a write happened in this snapshot (Rule 2 above).
            // Clear the value and output the SingleDelete. (The value will be
            // outputted on the next iteration.)

            // Setting valid_ to true will output the current SingleDelete
            validity_info_.SetValid(ValidContext::kKeepSDForConflictCheck);

            // Set up the Put to be outputted in the next iteration.
            // (Optimization 3).
            clear_and_output_next_key_ = true;
            TEST_SYNC_POINT_CALLBACK(
                "CompactionIterator::NextFromInput:KeepSDForWW",
                /*arg=*/nullptr);
          }
        } else {
          // We hit the next snapshot without hitting a put, so the iterator
          // returns the single delete.
          validity_info_.SetValid(ValidContext::kKeepSDForSnapshot);
          TEST_SYNC_POINT_CALLBACK(
              "CompactionIterator::NextFromInput:SingleDelete:3",
              const_cast<Compaction*>(c));
        }
      } else {
        // We are at the end of the input, could not parse the next key, or hit
        // a different key. The iterator returns the single delete if the key
        // possibly exists beyond the current output level.  We set
        // has_current_user_key to false so that if the iterator is at the next
        // key, we do not compare it again against the previous key at the next
        // iteration. If the next key is corrupt, we return before the
        // comparison, so the value of has_current_user_key does not matter.
        has_current_user_key_ = false;
        if (compaction_ != nullptr &&
            DefinitelyInSnapshot(ikey_.sequence, earliest_snapshot_) &&
            compaction_->KeyNotExistsBeyondOutputLevel(ikey_.user_key,
                                                       &level_ptrs_) &&
            is_timestamp_eligible_for_gc) {
          // Key doesn't exist outside of this range.
          // Can compact out this SingleDelete.
          CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
          compaction_tracer_->WriteDropKey(record);
          ++iter_stats_.num_record_drop_obsolete;
          ++iter_stats_.num_single_del_fallthru;
          if (!bottommost_level_) {
            ++iter_stats_.num_optimized_del_drop_obsolete;
          }
        } else if (last_key_seq_zeroed_) {
          // Skip.
          ++iter_stats_.num_record_drop_hidden;
          ++iter_stats_.num_record_drop_obsolete;
          CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
          compaction_tracer_->WriteDropKey(record);
          assert(bottommost_level_);
        } else {
          // Output SingleDelete
          validity_info_.SetValid(ValidContext::kKeepSD);
        }
      }

      if (Valid()) {
        at_next_ = true;
      }
    } else if (last_snapshot == current_user_key_snapshot_ ||
               (last_snapshot > 0 &&
                last_snapshot < current_user_key_snapshot_)) {
      // if last_snapshot > 0 && last_snapshot < kMaxSequenceNumber, then
      // current user key is the same as previous one.
      //  So key with smaller snapshot should be preserved.
      //   
      // If the earliest snapshot is which this key is visible in
      // is the same as the visibility of a previous instance of the
      // same key, then this kv is not visible in any snapshot.
      // Hidden by an newer entry for same user key
      //
      // Note: Dropping this key will not affect TransactionDB write-conflict
      // checking since there has already been a record returned for this key
      // in this snapshot.
      if (last_sequence < current_user_key_sequence_) {
        ROCKS_LOG_FATAL(info_log_,
                        "key %s, last_sequence (%" PRIu64
                        ") < current_user_key_sequence_ (%" PRIu64 ")",
                        ikey_.DebugString(allow_data_in_errors_, true).c_str(),
                        last_sequence, current_user_key_sequence_);
        assert(false);
      }
      // write to stderr without buffering print 
      // fprintf(stderr, "Hidden by newer entry for same user key, last_snapshot: %ld, current_user_key_snapshot_: %ld\n", last_snapshot, current_user_key_snapshot_);
      CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
      compaction_tracer_->WriteDropKey(record);

      ++iter_stats_.num_record_drop_hidden;  // rule (A)
      AdvanceInputIter();
    } else if (compaction_ != nullptr &&
               (ikey_.type == kTypeDeletion ||
                (ikey_.type == kTypeDeletionWithTimestamp &&
                 cmp_with_history_ts_low_ < 0)) &&
               DefinitelyInSnapshot(ikey_.sequence, earliest_snapshot_) &&
               compaction_->KeyNotExistsBeyondOutputLevel(ikey_.user_key,
                                                          &level_ptrs_)) {
      // printf("Key deletion & doesn't exist outside of this range\n");
      // TODO(noetzli): This is the only place where we use compaction_
      // (besides the constructor). We should probably get rid of this
      // dependency and find a way to do similar filtering during flushes.
      //
      // For this user key:
      // (1) there is no data in higher levels
      // (2) data in lower levels will have larger sequence numbers
      // (3) data in layers that are being compacted here and have
      //     smaller sequence numbers will be dropped in the next
      //     few iterations of this loop (by rule (A) above).
      // Therefore this deletion marker is obsolete and can be dropped.
      //
      // Note:  Dropping this Delete will not affect TransactionDB
      // write-conflict checking since it is earlier than any snapshot.
      //
      // It seems that we can also drop deletion later than earliest snapshot
      // given that:
      // (1) The deletion is earlier than earliest_write_conflict_snapshot, and
      // (2) No value exist earlier than the deletion.
      //
      // Note also that a deletion marker of type kTypeDeletionWithTimestamp
      // will be treated as a different user key unless the timestamp is older
      // than *full_history_ts_low_.
      ++iter_stats_.num_record_drop_obsolete;
      CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
      compaction_tracer_->WriteDropKey(record);
      // fprintf(stdout,"Obsolete due to earlier  delete\n");

      if (!bottommost_level_) {
        ++iter_stats_.num_optimized_del_drop_obsolete;
      }
      AdvanceInputIter();
    } else if ((ikey_.type == kTypeDeletion ||
                (ikey_.type == kTypeDeletionWithTimestamp &&
                 cmp_with_history_ts_low_ < 0)) &&
               bottommost_level_) {
      printf("bottom level delete\n");
      // Handle the case where we have a delete key at the bottom most level
      // We can skip outputting the key iff there are no subsequent puts for
      // this key
      assert(!compaction_ || compaction_->KeyNotExistsBeyondOutputLevel(
                                 ikey_.user_key, &level_ptrs_));
      ParsedInternalKey next_ikey;
      AdvanceInputIter();
#ifndef NDEBUG
      const Compaction* c =
          compaction_ ? compaction_->real_compaction() : nullptr;
#endif
      TEST_SYNC_POINT_CALLBACK(
          "CompactionIterator::NextFromInput:BottommostDelete:1",
          const_cast<Compaction*>(c));
      // Skip over all versions of this key that happen to occur in the same
      // snapshot range as the delete.
      //
      // Note that a deletion marker of type kTypeDeletionWithTimestamp will be
      // considered to have a different user key unless the timestamp is older
      // than *full_history_ts_low_.
      while (!IsPausingManualCompaction() && !IsShuttingDown() &&
             input_.Valid() &&
             (ParseInternalKey(input_.key(), &next_ikey, allow_data_in_errors_)
                  .ok()) &&
             cmp_->EqualWithoutTimestamp(ikey_.user_key, next_ikey.user_key) &&
             (prev_snapshot == 0 ||
              DefinitelyNotInSnapshot(next_ikey.sequence, prev_snapshot))) {
        AdvanceInputIter();
      }
      // If you find you still need to output a row with this key, we need to
      // output the delete too
      if (input_.Valid() &&
          (ParseInternalKey(input_.key(), &next_ikey, allow_data_in_errors_)
               .ok()) &&
          cmp_->EqualWithoutTimestamp(ikey_.user_key, next_ikey.user_key)) {
        validity_info_.SetValid(ValidContext::kKeepDel);
        at_next_ = true;
      }
    } else if (ikey_.type == kTypeMerge) {
      assert(false);
      if (!merge_helper_->HasOperator()) {
        status_ = Status::InvalidArgument(
            "merge_operator is not properly initialized.");
        return;
      }

      pinned_iters_mgr_.StartPinning();

      // We know the merge type entry is not hidden, otherwise we would
      // have hit (A)
      // We encapsulate the merge related state machine in a different
      // object to minimize change to the existing flow.
      merge_until_status_ = merge_helper_->MergeUntil(
          &input_, range_del_agg_, prev_snapshot, bottommost_level_,
          allow_data_in_errors_, blob_fetcher_.get(), full_history_ts_low_,
          prefetch_buffers_.get(), &iter_stats_);
      merge_out_iter_.SeekToFirst();

      if (!merge_until_status_.ok() &&
          !merge_until_status_.IsMergeInProgress()) {
        status_ = merge_until_status_;
        return;
      } else if (merge_out_iter_.Valid()) {
        // NOTE: key, value, and ikey_ refer to old entries.
        //       These will be correctly set below.
        key_ = merge_out_iter_.key();
        value_ = merge_out_iter_.value();
        pik_status = ParseInternalKey(key_, &ikey_, allow_data_in_errors_);
        // MergeUntil stops when it encounters a corrupt key and does not
        // include them in the result, so we expect the keys here to valid.
        if (!pik_status.ok()) {
          ROCKS_LOG_FATAL(
              info_log_, "Invalid key %s in compaction. %s",
              allow_data_in_errors_ ? key_.ToString(true).c_str() : "hidden",
              pik_status.getState());
          assert(false);
        }
        // Keep current_key_ in sync.
        current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
        key_ = current_key_.GetInternalKey();
        ikey_.user_key = current_key_.GetUserKey();
        validity_info_.SetValid(ValidContext::kMerge2);
      } else {
        // all merge operands were filtered out. reset the user key, since the
        // batch consumed by the merge operator should not shadow any keys
        // coming after the merges
        has_current_user_key_ = false;
        pinned_iters_mgr_.ReleasePinnedData();

        if (merge_helper_->FilteredUntil(&skip_until)) {
          need_skip = true;
        }
      }
    } else {
      // printf("new user key \n");
      // 1. new user key -OR-
      // 2. different snapshot stripe
      // If user-defined timestamp is enabled, we consider keys for GC if they
      // are below history_ts_low_. CompactionRangeDelAggregator::ShouldDelete()
      // only considers range deletions that are at or below history_ts_low_ and
      // trim_ts_. We drop keys here that are below history_ts_low_ and are
      // covered by a range tombstone that is at or below history_ts_low_ and
      // trim_ts.
      bool should_delete = false;
      if (!timestamp_size_ || cmp_with_history_ts_low_ < 0) {
        should_delete = range_del_agg_->ShouldDelete(
            key_, RangeDelPositioningMode::kForwardTraversal);
      }
      if (should_delete) {
        // printf("should  delete \n");
        fprintf(stdout, "should delete \n");
        CompactionTraceRecord record(env_->NowMicros(), input_.key().ToString());
        compaction_tracer_->WriteDropKey(record);

        ++iter_stats_.num_record_drop_hidden;
        ++iter_stats_.num_record_drop_range_del;
        AdvanceInputIter();
      } else {
        validity_info_.SetValid(ValidContext::kNewUserKey);
      }
    }

    if (need_skip) {
      // assert(false);
      SkipUntil(skip_until);
    }
  }

  if (!Valid() && IsShuttingDown()) {
    status_ = Status::ShutdownInProgress();
  }

  if (IsPausingManualCompaction()) {
    status_ = Status::Incomplete(Status::SubCode::kManualCompactionPaused);
  }

  // Propagate corruption status from memtable itereator
  if (!input_.Valid() && input_.status().IsCorruption()) {
    status_ = input_.status();
  }
}

bool CompactionIterator::ExtractLargeValueIfNeededImplWithLifetimeLabel(uint64_t lifetime_label, const Slice& key_meta_slice) {
  if(!lifetime_blob_file_builders_[lifetime_label]) {
    assert(false);
    return false;
  }
  // if (!blob_file_builder_) {
  //   return false;
  // }

  Status s;
  blob_index_.clear();
  // uint64_t bucket_id = booster_handle_->
   
  // s = lifetime_blob_file_builders_[lifetime_label]->Add(user_key(), value_, &blob_index_);

  //s = lifetime_blob_file_builders_[lifetime_label]->AddWithSeq(user_key(), value_, ikey().sequence, &blob_index_);

  s = lifetime_blob_file_builders_[lifetime_label]->Add(key(), value_, &blob_index_);


  if (!s.ok()) {
    status_ = s;
    validity_info_.Invalidate();
    assert(false);

    return false;
  }

  if (blob_index_.empty()) {
    assert(false);
    return false;
  }

  assert(key_meta_slice.size() > 0);
  blob_index_.append(key_meta_slice.data(), key_meta_slice.size());

  value_ = blob_index_;
  assert(value_.size() > 0);

  return true;
}


Status CompactionIterator::WriteTrainDataToFile(const std::vector<double>& data, double label) {
  Status s;
  if(!train_data_file_) {
    assert(false);
    return s;
  }
  for(size_t i = 0; i < data.size(); i++) {
    std::string data_with_sep = std::to_string(data[i]) + " ";
    s = train_data_file_->Append(data_with_sep);
    
  }
  if(!s.ok()) {
    return s;
  }
  std::string label_str = std::to_string(label) + "\n";
  s = train_data_file_->Append(label_str);
  return s;

}
bool CompactionIterator::GetFeatures( std::vector<double>* feature_vec ) {
  uint64_t seq=  ikey().sequence;
  // std::string user_key_str = user_key().ToString();
  std::string user_key_str = LDBCommand::StringToHex(user_key().ToString());
  assert(features_ != nullptr);
  // feature_vec = features_[user_key_str][seq];
  if(features_->find(user_key_str) == features_->end()) {
    assert(false);
    return false;
  }
  if(features_->at(user_key_str).find(seq) == features_->at(user_key_str).end()) {
    // assert(false);
    return false;
  }
  *feature_vec = std::move(features_->at(user_key_str).at(seq));
  return true;
  // SequenceNumber seq =  ikey().sequence; 
  //   bool get_feat = false;
  //   KeyFeatures* key_feat = nullptr;
  //   for(const auto &mem: *mems_) {

  //     get_feat = mem->GetKeyFeatures(seq, &key_feat);
  //     if(get_feat) {
  //       break;
  //     }
  //     // feature_vec-
  //   }
}
// Todo: Do prediction here  add value to the lifetime label blob file
bool CompactionIterator::ExtractLargeValueIfNeededImpl() {
  // if (!blob_file_builder_) {
  //   return false;
  // }
  if(lifetime_blob_file_builders_.size() == 0 || lifetime_blob_file_builders_[default_lifetime_idx_] == nullptr) {
    assert(false);
    return false;
  }

  Status s;
  blob_index_.clear();
    // uint64_t bucket_id = booster_handle_->
  if (blob_file_builder_) {
    assert(false);
    s = blob_file_builder_->Add(user_key(), value_, &blob_index_);
  } else {
    bool is_iter_valid = false;
    if(db_internal_iter_) {
      // db_internal_iter_->Seek(key());
      // is_iter_valid = db_internal_iter_->Valid(); 
    }
    // int maxIndex = std::min(db_->GetDefaultLifetimeIndex(), LifetimeSequence.size() -1 );
    int maxIndex = 2;
    // uint32_t past_distances_count = 0;
    // std::vector<uint64_t> past_distances;
    // past_distances.reserve(max_n_past_timestamps);
    // std::vector<float> edcs;
    // edcs.reserve(n_edc_feature);
    // uint64_t past_seq;
    // uint64_t distance = 0;
    if(is_iter_valid && db_internal_iter_->user_key().compare(user_key()) == 0) {
      assert(false);
    } else {
      // no previous key write  
      // write past_distances_count = 0 to file.
      // No need to write past_distances and edcs to value 
      // assert(past_distances_count == 0);
    }
   
    lifetime_keys_count_[maxIndex] += 1;
    s = lifetime_blob_file_builders_[maxIndex]->Add(key(), value_, &blob_index_);
  }

  if (!s.ok()) {
    status_ = s;
    validity_info_.Invalidate();
    assert(false);

    return false;
  }

  if (blob_index_.empty()) {
    assert(false);
    return false;
  }

  value_ = blob_index_;

  return true;
}

void CompactionIterator::ExtractLargeValueIfNeeded() {
  assert(ikey_.type == kTypeValue);

  if (!ExtractLargeValueIfNeededImpl()) {
    return;
  }

  ikey_.type = kTypeBlobIndex;
  current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);
}

uint64_t CompactionIterator::GetNewLifetimeLabel(std::shared_ptr<BlobFileMetaData> blob_file_meta, uint64_t cur_seq) {
  return 0;

}
uint64_t CompactionIterator::GetLifetimeLabelFromTTL(uint64_t orig_lifetime_label, uint64_t time_elapse_micros) {
  uint64_t orig_time_sec = LifetimeLabelToSecMap.at(orig_lifetime_label);
  // assert(orig_time_sec >= time_elapse_micros / 1000000);
  uint64_t remaining_time = 0;
  uint64_t sec_micros = 1000000;
  if(orig_time_sec < time_elapse_micros / sec_micros) {
    remaining_time = 0;
  } else{
    remaining_time = orig_time_sec - time_elapse_micros / sec_micros;
  }
  auto lower_boud_iter = std::lower_bound(LifetimeSecs.begin(), LifetimeSecs.end(), remaining_time);

  uint64_t new_lifetime_label=0;
  if(lower_boud_iter == LifetimeSecs.end()) {

    new_lifetime_label = LifetimeSecs.size() - 1;
    // fprintf(stderr, "Error: cannot find the lifetime label for the remaining time %lu \n", remaining_time);
    // assert(false);
  } else {
    new_lifetime_label = std::distance(LifetimeSecs.begin(), lower_boud_iter);
  }

  return new_lifetime_label; 
}

Slice CompactionIterator::CollectKeyFeatures(Slice orig_blob_index_slice) {
  key_meta_str_.clear();
  // this means that this key is newly inserted in flush.
  // We need to add past_distance_count to it.
  bool is_iter_valid = false;
  if(db_internal_iter_) {
    db_internal_iter_->Seek(key());
    is_iter_valid = db_internal_iter_->Valid(); 
  }
  if(!is_iter_valid) {
    PutVarint32(&key_meta_str_, 0);
    return key_meta_str_;
  }
  ParsedInternalKey parsed_previous_key;
  Status s = ParseInternalKey(db_internal_iter_->key(), &parsed_previous_key, false);
  if(!s.ok()) {
    assert(false);
  }
  uint32_t past_distances_count = 0;
  std::vector<uint64_t> past_distances;
  past_distances.reserve(max_n_past_distances);
  std::vector<float> edcs;
  edcs.reserve(n_edc_feature);
  uint64_t past_seq;
  uint64_t distance = 0;

  if(is_iter_valid && parsed_previous_key.user_key.compare(user_key()) ==0 ) {
    if(  parsed_previous_key.sequence < ikey().sequence  ) {
      bool get_feat = false;
      BlobIndex prev_blob_index;
      Slice prev_value = db_internal_iter_->value();
      uint64_t blob_index_len = 0;
      s = prev_blob_index.DecodeFromWithKeyMeta(prev_value, &blob_index_len);
      assert(s.ok());
      prev_value.remove_prefix(blob_index_len);
      bool ok = GetVarint32(&prev_value, &past_distances_count);

      if(!ok) {
        assert(false);
      }
      past_seq = parsed_previous_key.sequence;
      distance = ikey().sequence - past_seq;
      assert(distance > 0);

      std::vector<double> data_to_train;
      uint32_t this_past_distance = 0;
      uint8_t n_within = 0;
      uint32_t i = 0;
      assert(past_distances_count <= max_n_past_distances);

      for(i=0; i < past_distances_count && i < max_n_past_distances; i++) {

        uint64_t past_distance;
        ok = GetVarint64(&prev_value, &past_distance);
        past_distances.emplace_back(past_distance);
        this_past_distance +=  past_distance;  
        if(this_past_distance < memory_window) {
          ++n_within;
        }
        uint64_t distance_edc_window_idx = std::lower_bound(edc_windows.begin(), edc_windows.end(), past_distance) - edc_windows.begin();
        data_to_train.emplace_back(static_cast<double>(distance_edc_window_idx));
        assert(ok);
      }

      uint64_t blob_size = prev_blob_index.size();
      data_to_train.emplace_back(static_cast<double>(blob_size));

      data_to_train.emplace_back(static_cast<double>(n_within));

      if(past_distances_count> 0) {
        for(i=0; i < n_edc_feature; i++) {
          float edc;
          ok = GetFixed32(&prev_value, reinterpret_cast<uint32_t*>(&edc));
          edcs.emplace_back(edc);
          data_to_train.emplace_back(edc);
          assert(ok);
        }
      } else {
        for(i=0; i < n_edc_feature; i++) {
          uint32_t _distance_idx = std::min(uint32_t(distance / edc_windows[i]), max_hash_edc_idx);
          float edc = hash_edc[_distance_idx]   ;
          edcs.emplace_back(edc);
          data_to_train.emplace_back(edc);
        }
      }

      // db_->CDFAddLifetime(distance);
      db_->HistogramAddLifetime(distance);
      //update edcs
      // Need to set up edcs if past_distances_count = 0
      if(distance > db_->GetShortLifetimeThreshold()) {
        // Model prediction
        // This is not good as well. Any better solution ?
        // double inverse_distance = 1.0 / double(edcs[0]);
        double inverse_distance = db_->GetSampleRatio(edcs);
        // add training sample with inverse_distance probability
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1);
        double prob = dis(gen);
        if(prob < inverse_distance) {
          data_to_train.emplace_back(static_cast<double>(distance));
          s = db_->AddTrainingSample(data_to_train);
          assert(s.ok()); 
        }
      }

      if(past_distances_count > 0) {
        assert(edcs.size() == n_edc_feature);
        for(size_t k=0; k < n_edc_feature; k++) {
          uint32_t _distance_idx = std::min(uint32_t(distance / edc_windows[k]), max_hash_edc_idx);
          edcs[k] = edcs[k] * hash_edc[_distance_idx] + 1;
        }
      } else {
        // assert(edcs.size() == 0);
        for(size_t k=0; k < n_edc_feature; k++) {
          uint32_t _distance_idx = std::min(uint32_t(distance / edc_windows[k]), max_hash_edc_idx);
          float new_edc = hash_edc[_distance_idx]  + 1;
          edcs[k] = new_edc;
        }
      }
      // count new past_distances
      past_distances_count += 1;
    } else {
      assert(false);
    }
  } else {
    assert(past_distances_count == 0);
  }

  past_distances_count = std::min(past_distances_count, static_cast<uint32_t>(max_n_past_distances));
  PutVarint32(&key_meta_str_, past_distances_count);
  if(past_distances_count > 0) {
    PutVarint64(&key_meta_str_, distance ); 
    // write past_distances to value
    for(size_t i = 0; i < past_distances_count-1; i++) {
      PutVarint64(&key_meta_str_, past_distances[i]);
    }
    // write edcs to value
    // Need to update edcs
    for(int i = 0; i < n_edc_feature; i++) {
      PutFixed32(&key_meta_str_, *reinterpret_cast<uint32_t*>(&edcs[i]));
    }
  }
  Slice res(key_meta_str_.data() , key_meta_str_.size());
  return res;
}

void CompactionIterator::UpdateValueBlobIndexIfNeeded() {

  // This must be called in compaction job.
  // So db_internal_iter must not have iterator of level 0
  assert(ikey_.type == kTypeBlobIndex);
  Slice orig_blob_index_slice = value_;
  assert(compaction_);
  const Version* const version = compaction_.get()->input_version();
  BlobIndex orig_blob_index;
  uint64_t orig_blob_index_len=0;
  Status s= orig_blob_index.DecodeFromWithKeyMeta(orig_blob_index_slice, &orig_blob_index_len);
  assert(s.ok());
  
  const uint64_t key_meta_len = orig_blob_index_slice.size() - orig_blob_index_len;
  Slice res;
  if(key_meta_len == 0 ) {
    res = CollectKeyFeatures(orig_blob_index_slice);
  }

  Slice orig_blob_without_key_meta(orig_blob_index_slice.data() ,  orig_blob_index_len);
  Slice blob_key_meta;
  if(key_meta_len ==  0 ) {
    blob_key_meta = res;
    assert(blob_key_meta.size() > 0);
  } else {
    blob_key_meta = Slice(orig_blob_index_slice.data() + orig_blob_index_len, key_meta_len);
  }

  // Slice orig_blob_key_meta(orig_blob_index_slice.data() + orig_blob_index_len, orig_blob_index_slice.size() - orig_blob_index_len);
  Slice latest_blob_index_slice = version->GetLatestBlobIndex(orig_blob_index.file_number(),
                                                                       orig_blob_without_key_meta);
  
  if(latest_blob_index_slice.empty() || orig_blob_without_key_meta.compare(latest_blob_index_slice) == 0) {
    if(key_meta_len > 0) {
      return;
    }
    latest_blob_index_slice = orig_blob_without_key_meta;
  } else {
    num_blob_nidex_updated_++;  
  }
  blob_index_.clear();
  blob_index_.append(latest_blob_index_slice.data(), latest_blob_index_slice.size());
  blob_index_.append(blob_key_meta.data(), blob_key_meta.size());
  value_ = blob_index_;

}

void CompactionIterator::GarbageCollectBlobIfNeeded() {
  assert(ikey_.type == kTypeBlobIndex);

  if (!compaction_) {
    assert(false);
    return;
  }
  if(!compaction_->enable_blob_garbage_collection()) {
    assert(false);
    return;
  }

  return;
  // GC for integrated BlobDB
  if (compaction_->enable_blob_garbage_collection()) {
    TEST_SYNC_POINT_CALLBACK(
        "CompactionIterator::GarbageCollectBlobIfNeeded::TamperWithBlobIndex",
        &value_);

    BlobIndex blob_index;
    uint64_t blob_index_len;

    {

      // const Status s = blob_index.DecodeFrom(value_);
      const Status s = blob_index.DecodeFromWithKeyMeta(value_, &blob_index_len);

      if (!s.ok()) {
        status_ = s;
        validity_info_.Invalidate();

        return;
      }
    }
    Slice key_meta_slice(value_.data() + blob_index_len, value_.size() - blob_index_len);
    std::string key_meta_str(key_meta_slice.data(), key_meta_slice.size());
    // blob file creation timestamp
    //
    // blob file lifetime label
    auto vstorage = cfd_->current()->storage_info();


    auto blob_file_meta = vstorage->FastGetBlobFileMetaData(blob_index.file_number());
    uint64_t lifetime_label = blob_file_meta->GetLifetimeLabel();

    // uint64_t creation_timestamp = blob_file_meta->GetCreationTimestamp();

    // uint64_t time_elapse =  env_->NowMicros()  - creation_timestamp;
    // uint64_t new_lifetime_label = GetLifetimeLabelFromTTL(lifetime_label, time_elapse);

    // uint64_t new_lifetime_label = GetNewLifetimeLabel(blob_file_meta, ikey_.sequence);
    uint64_t new_lifetime_label = lifetime_label;


    bool should_gc = false;
    // if(compaction_ && 
    //   compaction_->real_compaction()->compaction_reason() == CompactionReason::kForcedBlobGC ) {

    //   should_gc = true;

    // }
    //
    // if(compaction_ && 
    //   compaction_->real_compaction()->compaction_reason() == CompactionReason::kForcedBlobGC 
    //   ) {
    //   assert(!gc_blob_files_.empty());
    //   if(gc_blob_files_.find(blob_index.file_number()) != gc_blob_files_.end()) {
    //     should_gc = true;
    //   }


    // }
    //
    if(compaction_ && gc_blob_files_.find(blob_index.file_number()) != gc_blob_files_.end()) {
      should_gc = true;
    }

    if(!should_gc) {
      return;
    }
    // if( !should_gc && lifetime_label == new_lifetime_label) {
    //   
    //   return;
    // }



    // if (!should_gc && blob_index.file_number() >=
    //     blob_garbage_collection_cutoff_file_number_) {
    //   return;
    // }


    FilePrefetchBuffer* prefetch_buffer =
        prefetch_buffers_ ? prefetch_buffers_->GetOrCreatePrefetchBuffer(
                                blob_index.file_number())
                          : nullptr;

    uint64_t bytes_read = 0;

    {
      assert(blob_fetcher_);

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

    value_ = blob_value_;

    if (ExtractLargeValueIfNeededImplWithLifetimeLabel(new_lifetime_label, key_meta_str)) {

      return;
    }

    assert(false);
    ikey_.type = kTypeValue;
    current_key_.UpdateInternalKey(ikey_.sequence, ikey_.type);

    return;
  }

  // GC for stacked BlobDB
  if (compaction_filter_ &&
      compaction_filter_->IsStackedBlobDbInternalCompactionFilter()) {
    const auto blob_decision = compaction_filter_->PrepareBlobOutput(
        user_key(), value_, &compaction_filter_value_);

    if (blob_decision == CompactionFilter::BlobDecision::kCorruption) {
      status_ =
          Status::Corruption("Corrupted blob reference encountered during GC");
      validity_info_.Invalidate();

      return;
    }

    if (blob_decision == CompactionFilter::BlobDecision::kIOError) {
      status_ = Status::IOError("Could not relocate blob during GC");
      validity_info_.Invalidate();

      return;
    }

    if (blob_decision == CompactionFilter::BlobDecision::kChangeValue) {
      value_ = compaction_filter_value_;

      return;
    }
  }
}

void CompactionIterator::DecideOutputLevel() {
  assert(compaction_->SupportsPerKeyPlacement());
#ifndef NDEBUG
  // Could be overridden by unittest
  PerKeyPlacementContext context(level_, ikey_.user_key, value_,
                                 ikey_.sequence);
  TEST_SYNC_POINT_CALLBACK("CompactionIterator::PrepareOutput.context",
                           &context);
  output_to_penultimate_level_ = context.output_to_penultimate_level;
#else
  output_to_penultimate_level_ = false;
#endif  // NDEBUG

  // if the key is newer than the cutoff sequence or within the earliest
  // snapshot, it should output to the penultimate level.
  if (ikey_.sequence > preclude_last_level_min_seqno_ ||
      ikey_.sequence > earliest_snapshot_) {
    output_to_penultimate_level_ = true;
  }

  if (output_to_penultimate_level_) {
    // If it's decided to output to the penultimate level, but unsafe to do so,
    // still output to the last level. For example, moving the data from a lower
    // level to a higher level outside of the higher-level input key range is
    // considered unsafe, because the key may conflict with higher-level SSTs
    // not from this compaction.
    // TODO: add statistic for declined output_to_penultimate_level
    bool safe_to_penultimate_level =
        compaction_->WithinPenultimateLevelOutputRange(ikey_.user_key);
    if (!safe_to_penultimate_level) {
      output_to_penultimate_level_ = false;
      // It could happen when disable/enable `last_level_temperature` while
      // holding a snapshot. When `last_level_temperature` is not set
      // (==kUnknown), the data newer than any snapshot is pushed to the last
      // level, but when the per_key_placement feature is enabled on the fly,
      // the data later than the snapshot has to be moved to the penultimate
      // level, which may or may not be safe. So the user needs to make sure all
      // snapshot is released before enabling `last_level_temperature` feature
      // We will migrate the feature to `last_level_temperature` and maybe make
      // it not dynamically changeable.
      if (ikey_.sequence > earliest_snapshot_) {
        status_ = Status::Corruption(
            "Unsafe to store Seq later than snapshot in the last level if "
            "per_key_placement is enabled");
      }
    }
  }
}

void CompactionIterator::PrepareOutput() {
  if (Valid()) {
    if (ikey_.type == kTypeValue) {
      ExtractLargeValueIfNeeded();
    } else if (ikey_.type == kTypeBlobIndex) {
       UpdateValueBlobIndexIfNeeded();
      // GarbageCollectBlobIfNeeded();
    }

    if (compaction_ != nullptr && compaction_->SupportsPerKeyPlacement()) {
      DecideOutputLevel();
    }

    // no zeroing sequence number
    return;

    // Zeroing out the sequence number leads to better compression.
    // If this is the bottommost level (no files in lower levels)
    // and the earliest snapshot is larger than this seqno
    // and the userkey differs from the last userkey in compaction
    // then we can squash the seqno to zero.
    //
    // This is safe for TransactionDB write-conflict checking since transactions
    // only care about sequence number larger than any active snapshots.
    //
    // Can we do the same for levels above bottom level as long as
    // KeyNotExistsBeyondOutputLevel() return true?
    if (Valid() && compaction_ != nullptr &&
        !compaction_->allow_ingest_behind() && bottommost_level_ &&
        DefinitelyInSnapshot(ikey_.sequence, earliest_snapshot_) &&
        ikey_.type != kTypeMerge && current_key_committed_ &&
        !output_to_penultimate_level_ &&
        ikey_.sequence < preserve_time_min_seqno_) {
      if (ikey_.type == kTypeDeletion ||
          (ikey_.type == kTypeSingleDeletion && timestamp_size_ == 0)) {
        ROCKS_LOG_FATAL(
            info_log_,
            "Unexpected key %s for seq-zero optimization. "
            "earliest_snapshot %" PRIu64
            ", earliest_write_conflict_snapshot %" PRIu64
            " job_snapshot %" PRIu64
            ". timestamp_size: %d full_history_ts_low_ %s. validity %x",
            ikey_.DebugString(allow_data_in_errors_, true).c_str(),
            earliest_snapshot_, earliest_write_conflict_snapshot_,
            job_snapshot_, static_cast<int>(timestamp_size_),
            full_history_ts_low_ != nullptr
                ? Slice(*full_history_ts_low_).ToString(true).c_str()
                : "null",
            validity_info_.rep);
        assert(false);
      }
      ikey_.sequence = 0;
      last_key_seq_zeroed_ = true;
      fprintf(stdout, "PrepareOutput: zero seq, current level is %d\n",
              level_);
      assert(false);
      TEST_SYNC_POINT_CALLBACK("CompactionIterator::PrepareOutput:ZeroingSeq",
                               &ikey_);
      if (!timestamp_size_) {
        current_key_.UpdateInternalKey(0, ikey_.type);
      } else if (full_history_ts_low_ && cmp_with_history_ts_low_ < 0) {
        // We can also zero out timestamp for better compression.
        // For the same user key (excluding timestamp), the timestamp-based
        // history can be collapsed to save some space if the timestamp is
        // older than *full_history_ts_low_.
        const std::string kTsMin(timestamp_size_, static_cast<char>(0));
        const Slice ts_slice = kTsMin;
        ikey_.SetTimestamp(ts_slice);
        current_key_.UpdateInternalKey(0, ikey_.type, &ts_slice);
      }
    }
  }
}

inline SequenceNumber CompactionIterator::findEarliestVisibleSnapshot(
    SequenceNumber in, SequenceNumber* prev_snapshot) {
  assert(snapshots_->size());
  if (snapshots_->size() == 0) {
    ROCKS_LOG_FATAL(info_log_,
                    "No snapshot left in findEarliestVisibleSnapshot");
  }
  auto snapshots_iter =
      std::lower_bound(snapshots_->begin(), snapshots_->end(), in);
  assert(prev_snapshot != nullptr);
  if (snapshots_iter == snapshots_->begin()) {
    *prev_snapshot = 0;
  } else {
    *prev_snapshot = *std::prev(snapshots_iter);
    if (*prev_snapshot >= in) {
      ROCKS_LOG_FATAL(info_log_,
                      "*prev_snapshot (%" PRIu64 ") >= in (%" PRIu64
                      ") in findEarliestVisibleSnapshot",
                      *prev_snapshot, in);
      assert(false);
    }
  }
  if (snapshot_checker_ == nullptr) {
    return snapshots_iter != snapshots_->end() ? *snapshots_iter
                                               : kMaxSequenceNumber;
  }
  bool has_released_snapshot = !released_snapshots_.empty();
  for (; snapshots_iter != snapshots_->end(); ++snapshots_iter) {
    auto cur = *snapshots_iter;
    if (in > cur) {
      ROCKS_LOG_FATAL(info_log_,
                      "in (%" PRIu64 ") > cur (%" PRIu64
                      ") in findEarliestVisibleSnapshot",
                      in, cur);
      assert(false);
    }
    // Skip if cur is in released_snapshots.
    if (has_released_snapshot && released_snapshots_.count(cur) > 0) {
      continue;
    }
    auto res = snapshot_checker_->CheckInSnapshot(in, cur);
    if (res == SnapshotCheckerResult::kInSnapshot) {
      return cur;
    } else if (res == SnapshotCheckerResult::kSnapshotReleased) {
      released_snapshots_.insert(cur);
    }
    *prev_snapshot = cur;
  }
  return kMaxSequenceNumber;
}

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

std::unique_ptr<BlobFetcher> CompactionIterator::CreateBlobFetcherIfNeeded(
    const CompactionProxy* compaction) {
  if (!compaction) {
    return nullptr;
  }

  const Version* const version = compaction->input_version();
  if (!version) {
    return nullptr;
  }

  ReadOptions read_options;
  read_options.fill_cache = false;

  return std::unique_ptr<BlobFetcher>(new BlobFetcher(version, read_options));
}

std::unique_ptr<PrefetchBufferCollection>
CompactionIterator::CreatePrefetchBufferCollectionIfNeeded(
    const CompactionProxy* compaction) {
  if (!compaction) {
    return nullptr;
  }

  if (!compaction->input_version()) {
    return nullptr;
  }

  if (compaction->allow_mmap_reads()) {
    return nullptr;
  }

  const uint64_t readahead_size = compaction->blob_compaction_readahead_size();
  if (!readahead_size) {
    return nullptr;
  }

  return std::unique_ptr<PrefetchBufferCollection>(
      new PrefetchBufferCollection(readahead_size));
}

}  // namespace ROCKSDB_NAMESPACE
