//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <string>

#include "db/blob/blob_constants.h"
#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

class JSONWriter;
class Slice;
class Status;

class BlobFileAddition {
 public:
  BlobFileAddition() = default;


  explicit BlobFileAddition(uint64_t blob_file_number, uint64_t total_blob_count,
                   uint64_t total_blob_bytes, std::string checksum_method,
                   std::string checksum_value, uint64_t lifetime_label,
                   uint64_t build_timestamp)
                   : BlobFileAddition(blob_file_number, total_blob_count,
                                      total_blob_bytes, checksum_method,
                                      checksum_value) {
    lifetime_label_ = lifetime_label;
    build_timestamp_ = build_timestamp;
  }


  void SetLifetime(int label, uint64_t timestamp) ;  

  uint64_t GetLifetimeLabel() const { return lifetime_label_; }
  uint64_t GetCreationTimestamp() const { return build_timestamp_; }

  uint64_t GetBlobFileNumber() const { return blob_file_number_; }
  uint64_t GetTotalBlobCount() const { return total_blob_count_; }
  uint64_t GetTotalBlobBytes() const { return total_blob_bytes_; }
  const std::string& GetChecksumMethod() const { return checksum_method_; }
  const std::string& GetChecksumValue() const { return checksum_value_; }

  void EncodeTo(std::string* output) const;
  Status DecodeFrom(Slice* input);

  std::string DebugString() const;
  std::string DebugJSON() const;

 private:
  BlobFileAddition(uint64_t blob_file_number, uint64_t total_blob_count,
                   uint64_t total_blob_bytes, std::string checksum_method,
                   std::string checksum_value)
      : blob_file_number_(blob_file_number),
        total_blob_count_(total_blob_count),
        total_blob_bytes_(total_blob_bytes),
        checksum_method_(std::move(checksum_method)),
        checksum_value_(std::move(checksum_value)) {
    assert(checksum_method_.empty() == checksum_value_.empty());
  }
  enum CustomFieldTags : uint32_t;

  uint64_t blob_file_number_ = kInvalidBlobFileNumber;
  uint64_t total_blob_count_ = 0;
  uint64_t total_blob_bytes_ = 0;
  int lifetime_label_ = -1;
  uint64_t lifetime_duration_in_seconds_ = 0;
  uint64_t build_timestamp_ = 0;
  std::string checksum_method_;
  std::string checksum_value_;
  // uint64_t creation_timestamp_ = 0;
};

bool operator==(const BlobFileAddition& lhs, const BlobFileAddition& rhs);
bool operator!=(const BlobFileAddition& lhs, const BlobFileAddition& rhs);

std::ostream& operator<<(std::ostream& os,
                         const BlobFileAddition& blob_file_addition);
JSONWriter& operator<<(JSONWriter& jw,
                       const BlobFileAddition& blob_file_addition);

}  // namespace ROCKSDB_NAMESPACE
