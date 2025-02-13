//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
#pragma once

#include <cinttypes>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "rocksdb/advanced_options.h"
#include "rocksdb/compression_type.h"
#include "rocksdb/env.h"
#include "rocksdb/rocksdb_namespace.h"
#include "rocksdb/types.h"

namespace ROCKSDB_NAMESPACE {

class VersionSet;
class FileSystem;
class SystemClock;
struct ImmutableOptions;
struct MutableCFOptions;
struct FileOptions;
class BlobFileAddition;
class Status;
class Slice;
class BlobLogWriter;
class IOTracer;
class BlobFileCompletionCallback;

class BlobFileBuilder {
 public:
  BlobFileBuilder(VersionSet* versions, FileSystem* fs,
                  const ImmutableOptions* immutable_options,
                  const MutableCFOptions* mutable_cf_options,
                  const FileOptions* file_options, std::string db_id,
                  std::string db_session_id, int job_id,
                  uint32_t column_family_id,
                  const std::string& column_family_name,
                  Env::IOPriority io_priority,
                  Env::WriteLifeTimeHint write_hint,
                  const std::shared_ptr<IOTracer>& io_tracer,
                  BlobFileCompletionCallback* blob_callback,
                  BlobFileCreationReason creation_reason,
                  std::vector<std::string>* blob_file_paths,
                  std::vector<BlobFileAddition>* blob_file_additions,
                  uint64_t lifetime_label=0,
                  uint64_t creation_timestamp=0,
                  uint64_t ending_timestamp=0);

  BlobFileBuilder(std::function<uint64_t()> file_number_generator,
                  FileSystem* fs, const ImmutableOptions* immutable_options,
                  const MutableCFOptions* mutable_cf_options,
                  const FileOptions* file_options, std::string db_id,
                  std::string db_session_id, int job_id,
                  uint32_t column_family_id,
                  const std::string& column_family_name,
                  Env::IOPriority io_priority,
                  Env::WriteLifeTimeHint write_hint,
                  const std::shared_ptr<IOTracer>& io_tracer,
                  BlobFileCompletionCallback* blob_callback,
                  BlobFileCreationReason creation_reason,
                  std::vector<std::string>* blob_file_paths,
                  std::vector<BlobFileAddition>* blob_file_additions,
                  uint64_t lifetime_label=0,
                  uint64_t creation_timestamp=0,
                  uint64_t ending_timestamp=0);

  BlobFileBuilder(const BlobFileBuilder&) = delete;
  BlobFileBuilder& operator=(const BlobFileBuilder&) = delete;

  ~BlobFileBuilder();

  Status AddWithSeq(const Slice& key, const Slice& value, uint64_t seq,  std::string* blob_index);
  Status Add(const Slice& key, const Slice& value, std::string* blob_index);
  Status Add(const Slice& key, const Slice& value, std::string* blob_index, uint64_t blob_lifetime_bucket);
  Status AddWithoutBlobFileClose(const Slice& key, const Slice& value, std::string* blob_index);
  Status Finish();
  Status Finish(uint64_t creation_timestamp);
  uint64_t GetLifetimeLabel() const { return lifetime_label_; }
  uint64_t GetCreationTimestamp() const { return creation_timestamp_; }
  void Abandon(const Status& s);

  Status CloseBlobFileIfNeeded();
  Status CloseBlobFileIfNeeded(uint64_t creation_timestamp);

  uint64_t GetCurBlobFileNum() const { return cur_blob_file_num_; }

  bool IsBlobFileOpen() const;
 private:
  Status OpenBlobFileIfNeeded();
  Status CompressBlobIfNeeded(Slice* blob, std::string* compressed_blob) const;
  Status WriteBlobToFile(const Slice& key, const Slice& blob,
                         uint64_t* blob_file_number, uint64_t* blob_offset);
  Status CloseBlobFile();

  Status PutBlobIntoCacheIfNeeded(const Slice& blob, uint64_t blob_file_number,
                                  uint64_t blob_offset) const;

  std::function<uint64_t()> file_number_generator_;
  FileSystem* fs_;
  const ImmutableOptions* immutable_options_;
  uint64_t min_blob_size_;
  uint64_t blob_file_size_;
  CompressionType blob_compression_type_;
  PrepopulateBlobCache prepopulate_blob_cache_;
  const FileOptions* file_options_;
  const std::string db_id_;
  const std::string db_session_id_;
  int job_id_;
  uint32_t column_family_id_;
  std::string column_family_name_;
  Env::IOPriority io_priority_;
  Env::WriteLifeTimeHint write_hint_;
  std::shared_ptr<IOTracer> io_tracer_;
  BlobFileCompletionCallback* blob_callback_;
  BlobFileCreationReason creation_reason_;
  std::vector<std::string>* blob_file_paths_;
  std::vector<BlobFileAddition>* blob_file_additions_;
  std::unique_ptr<BlobLogWriter> writer_;
  std::vector<std::unique_ptr<BlobLogWriter>> lifetime_writers_;
  uint64_t blob_count_;
  uint64_t blob_bytes_;
  uint64_t lifetime_label_;  
  uint64_t creation_timestamp_;
  uint64_t ending_timestamp_;
  uint64_t lifetime_bucket_num_;
  uint64_t min_seq_;
  uint64_t max_seq_;

  uint64_t cur_blob_file_num_ = 0;
};

}  // namespace ROCKSDB_NAMESPACE
