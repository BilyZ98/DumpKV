#include "db/compaction/garbage_collection_picker.h"


#include <cinttypes>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "db/column_family.h"
#include "file/filename.h"
#include "logging/log_buffer.h"
#include "logging/logging.h"
#include "monitoring/statistics.h"
#include "test_util/sync_point.h"
#include "util/random.h"
#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {

GarbageCollectionPicker::GarbageCollectionPicker(const ImmutableOptions& ioptions)
  : ioptions_(ioptions) {}


GarbageCollectionPicker::~GarbageCollectionPicker() {}

GarbageCollection* GarbageCollectionPicker::PickGarbageCollection(
      const std::string& cf_name, 
      const MutableCFOptions& mutable_cf_options,
      const MutableDBOptions& mutable_db_options,
      VersionStorageInfo* vstorage,
      LogBuffer* log_buffer)  {
  input_blob_files_.clear();

  for(auto& blob_file: vstorage->BlobFilesMarkedForForcedBlobGC()) {
    // assert(!blob_file->GetBeingGCed());
    if(blob_file->GetBeingGCed()) {
      continue;
    }
    input_blob_files_.push_back(blob_file);

    // blob_file->SetBeingGCed();
  }

  if(input_blob_files_.empty()) {
    return nullptr;
  }
  GarbageCollection* garbage_collection = new GarbageCollection(
      vstorage, ioptions_, mutable_cf_options, mutable_db_options, input_blob_files_);

  RegisterGarbageCollection(garbage_collection);

  // vstorage->ComputeBlobsMarkedForForcedGC( mutable_cf_options.blob_garbage_collection_age_cutoff) ;

  return garbage_collection;

}


bool GarbageCollectionPicker::NeedsGarbageCollection(const VersionStorageInfo* vstorage) const  {
  return !vstorage->BlobFilesMarkedForForcedBlobGC().empty();

}
 
void GarbageCollectionPicker::ReleaseGarbageCollection(GarbageCollection* garbage_collection)  {
  UnregisterGarbageCollection(garbage_collection);
}
void GarbageCollectionPicker::UnregisterGarbageCollection(GarbageCollection* garbage_collection) {
  if(garbage_collection != nullptr) {
    garbage_collections_in_progress_.erase(garbage_collection);
  }
  // delete garbage_collection;
}


bool GarbageCollectionPicker::GCFilesInUsed(const BlobFiles* blob_files) const  {
  // for(const auto& gc: garbage_collections_in_progress_) {

  //   for(const auto& blob_file: *blob_files) {
  //     if(blob_file->GetBeingGCed()) {
  //       return true;
  //     }
  //   }

  // }
  //
  // return false;
  return false;

}
void GarbageCollectionPicker::RegisterGarbageCollection(GarbageCollection* garbage_collection) {
  // assert(!GCFilesInUsed(garbage_collection->inputs()));
  if(garbage_collection != nullptr) {
    garbage_collections_in_progress_.insert(garbage_collection);
  }
}



}
