// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_RANGE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_RANGE_H

#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/internal/hash_validator.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include "google/storage/v2/storage.pb.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A read range represents a partially completed range download via a
 * `ObjectDescriptor`.
 *
 * An `ObjectDescriptor` may have many active ranges at a time. The data for
 * them may be interleaved, that is, data for ranges requested first may arrive
 * second. The object descriptor implementation will demux these messages to an
 * instance of this class.
 */
class ReadRange {
 public:
   using ReadResponse = storage::AsyncReaderConnection::ReadResponse;
   using MetadataAccessor =
       std::function<absl::optional<google::storage::v2::Object>()>;

  ReadRange(std::int64_t offset, absl::optional<std::int64_t> requested_length,
            std::string bucket_name, std::string object_name,
            std::shared_ptr<storage::internal::HashFunction> hash_function =
                storage::internal::CreateNullHashFunction(),
            std::unique_ptr<storage::internal::HashValidator> hash_validator =
                storage::internal::CreateNullHashValidator(),
            MetadataAccessor metadata_accessor = [] { return absl::nullopt; })
      : offset_(offset),
        length_(requested_length.value_or(0)),
        requested_length_(requested_length),
        bucket_name_(std::move(bucket_name)),
        object_name_(std::move(object_name)),
        metadata_accessor_(std::move(metadata_accessor)),
        hash_function_(std::move(hash_function)),
        hash_validator_(std::move(hash_validator)) {}

  bool IsDone() const;

  absl::optional<google::storage::v2::ReadRange> RangeForResume(
      std::int64_t read_id) const;

  future<ReadResponse> Read();
  void OnFinish(Status status);

  void OnRead(google::storage::v2::ObjectRangeData data);

 private:
  void Notify(std::unique_lock<std::mutex> lk, storage::ReadPayload p);
  // Must be called while holding mu_.
  void CheckOverrun();

  mutable std::mutex mu_;
  std::int64_t offset_;
  std::int64_t length_;
  absl::optional<std::int64_t> requested_length_;
  std::int64_t received_bytes_ = 0;
  std::string bucket_name_;
  std::string object_name_;
  MetadataAccessor metadata_accessor_;
  bool logged_warning_ = false;
  absl::optional<storage::ReadPayload> payload_;
  absl::optional<Status> status_;
  absl::optional<promise<ReadResponse>> wait_;
  std::shared_ptr<storage::internal::HashFunction> hash_function_;
  std::unique_ptr<storage::internal::HashValidator> hash_validator_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_RANGE_H
