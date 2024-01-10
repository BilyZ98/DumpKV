
#pragma once
#include "db/version_set.h"
#include "memory/arena.h"
#include "options/cf_options.h"
#include "rocksdb/sst_partitioner.h"
#include "util/autovector.h"

#include "db/version_set.h"

namespace ROCKSDB_NAMESPACE {

class Version;
class ColumnFamilyData;
class VersionStorageInfo;
class CompactionFilter;


using BlobFiles = std::vector<std::shared_ptr<BlobFileMetaData>>;
class GarbageCollection {
public:
  GarbageCollection(VersionStorageInfo* input_version,
                 const ImmutableOptions& immutable_options,
                 const MutableCFOptions& mutable_cf_options,
                 const MutableDBOptions& mutable_db_options,
                 BlobFiles blob_files);

  ~GarbageCollection();
  // Return the object that holds the edits to the descriptor done
  // by this compaction.
  VersionEdit* edit() { return &edit_; }

  // Returns input version of the compaction
  Version* input_version() const { return input_version_; }

  const BlobFiles* inputs() { return &input_blob_files_; }

  // Returns the ColumnFamilyData associated with the compaction.
  ColumnFamilyData* column_family_data() const { return cfd_; }

  // Returns the summary of the compaction in "output" with maximum "len"
  // in bytes.  The caller is responsible for the memory management of
  // "output".
  void Summary(char* output, int len);

  // Return the ImmutableOptions that should be used throughout the compaction
  // procedure
  const ImmutableOptions* immutable_options() const {
    return &immutable_options_;
  }

  // Return the MutableCFOptions that should be used throughout the compaction
  // procedure
  const MutableCFOptions* mutable_cf_options() const {
    return &mutable_cf_options_;
  }

  void SetInputVersion(Version* input_version);

private:
  // No copying allowed
  GarbageCollection(const GarbageCollection&);
  void operator=(const GarbageCollection&);
  const BlobFiles input_blob_files_;
  VersionStorageInfo* input_vstorage_;
  const ImmutableOptions immutable_options_;
  const MutableCFOptions mutable_cf_options_;
  Version* input_version_;
  VersionEdit edit_;
  ColumnFamilyData* cfd_;
  Arena arena_;  // Arena used to allocate space for file_levels_


};




}
