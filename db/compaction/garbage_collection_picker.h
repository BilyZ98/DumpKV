
#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "db/compaction/garbage_collection.h"
#include "db/version_set.h"
#include "options/cf_options.h"
#include "rocksdb/env.h"
#include "rocksdb/options.h"
#include "rocksdb/status.h"

namespace ROCKSDB_NAMESPACE {

// The file contains an abstract class CompactionPicker, and its two
// sub-classes LevelCompactionPicker and NullCompactionPicker, as
// well as some helper functions used by them.

class LogBuffer;
// class Compaction;
class VersionStorageInfo;
class GarbageCollection;
struct CompactionInputFiles;


class GarbageCollectionPicker {
public:
  GarbageCollectionPicker(const ImmutableOptions& ioptions);
  virtual ~GarbageCollectionPicker() ;

  virtual GarbageCollection* PickGarbageCollection(
      const std::string& cf_name, 
      const MutableCFOptions& mutable_cf_options,
      const MutableDBOptions& mutable_db_options,
      VersionStorageInfo* vstorage,
      LogBuffer* log_buffer) ;


  virtual bool NeedsGarbageCollection(const VersionStorageInfo* vstorage) const ;
  void ReleaseGarbageCollection(GarbageCollection* garbage_collection) ;

  void UnregisterGarbageCollection(GarbageCollection* garbage_collection) ;
  void RegisterGarbageCollection(GarbageCollection* garbage_collection) ;
  bool GCFilesInUsed(const BlobFiles* blob_files) const ;
private:
  ImmutableOptions ioptions_;
  std::unordered_set<GarbageCollection*> garbage_collections_in_progress_;
  BlobFiles input_blob_files_;

};

}
