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
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/storage/v2/storage.pb.h>
#include <cstdint>
#include <memory>
#include <mutex>

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
  using ReadResponse =
      storage_experimental::AsyncReaderConnection::ReadResponse;

  ReadRange(std::int64_t offset, std::int64_t length,
            std::shared_ptr<storage::internal::HashFunction> hash_function =
                storage::internal::CreateNullHashFunction())
      : offset_(offset),
        length_(length),
        hash_function_(std::move(hash_function)) {}

  bool IsDone() const;

  absl::optional<google::storage::v2::ReadRange> RangeForResume(
      std::int64_t read_id) const;

  future<ReadResponse> Read();
  void OnFinish(Status status);

  void OnRead(google::storage::v2::ObjectRangeData data);

 private:
  void Notify(std::unique_lock<std::mutex> lk,
              storage_experimental::ReadPayload p);

  mutable std::mutex mu_;
  std::int64_t offset_;
  std::int64_t length_;
  absl::optional<storage_experimental::ReadPayload> payload_;
  absl::optional<Status> status_;
  absl::optional<promise<ReadResponse>> wait_;
  std::shared_ptr<storage::internal::HashFunction> hash_function_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_RANGE_H
