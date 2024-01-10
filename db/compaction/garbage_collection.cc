#include "db/compaction/garbage_collection.h"

#include <cinttypes>
#include <vector>

#include "db/column_family.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/sst_partitioner.h"
#include "test_util/sync_point.h"
#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {


GarbageCollection::GarbageCollection(VersionStorageInfo* input_version,
                 const ImmutableOptions& immutable_options,
                 const MutableCFOptions& mutable_cf_options,
                 const MutableDBOptions& mutable_db_options,
                 BlobFiles blob_files)
: input_blob_files_(blob_files),
  input_vstorage_(input_version),
  immutable_options_(immutable_options),
  mutable_cf_options_(mutable_cf_options),
  input_version_(nullptr),
  cfd_(nullptr) {

}

void GarbageCollection::Summary(char* output, int len) {

}
void GarbageCollection::SetInputVersion(Version* input_version) {
  input_version_ = input_version;
  cfd_ = input_version_->cfd();

  input_version_->Ref();
  cfd_->Ref();
  edit_.SetColumnFamily(cfd_->GetID());
}

GarbageCollection::~GarbageCollection() {
  if (input_version_ != nullptr) {
    input_version_->Unref();
  }
  if (cfd_ != nullptr) {
    cfd_->UnrefAndTryDelete();
  }


}

}
