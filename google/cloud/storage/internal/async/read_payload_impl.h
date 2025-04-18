// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_PAYLOAD_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_PAYLOAD_IMPL_H

#include "google/cloud/storage/async/object_responses.h"
#include "google/cloud/storage/internal/async/read_payload_fwd.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ReadPayloadImpl {
  /**
   * Factory function for `ReadPayload` consuming `absl::Cord`.
   *
   * We don't want a public constructor, because `absl::Cord` is not stable
   * enough for public APIs.
   */
  static storage_experimental::ReadPayload Make(absl::Cord contents) {
    return storage_experimental::ReadPayload(std::move(contents));
  }

  /**
   * Factory function for `ReadPayload` consuming `std::string`.
   *
   * There is a public constructor, but we want to simplify some code that
   * uses `std::string` or `absl::Cord` depending on how the protos were
   * compiled.
   */
  static storage_experimental::ReadPayload Make(std::string contents) {
    return storage_experimental::ReadPayload(std::move(contents));
  }

  /// Append the data from @p rhs to @p lhs.
  static void Accumulate(storage_experimental::ReadPayload& lhs,
                         storage_experimental::ReadPayload&& rhs) {
    if (lhs.impl_.empty()) {
      lhs = std::move(rhs);
      return;
    }
    lhs.impl_.Append(std::move(rhs.impl_));
  }

  /// Get the object hashes (by move) from the payload.
  static absl::optional<storage::internal::HashValues> GetObjectHashes(
      storage_experimental::ReadPayload& payload) {
    return std::move(payload.object_hash_values_);
  }

  /// Set the object hashes in the payload.
  static void SetObjectHashes(storage_experimental::ReadPayload& payload,
                              storage::internal::HashValues hashes) {
    payload.object_hash_values_ = std::move(hashes);
  }

  static void Append(storage_experimental::ReadPayload& payload,
                     storage_experimental::ReadPayload new_data) {
    payload.impl_.Append(std::move(new_data.impl_));
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_READ_PAYLOAD_IMPL_H
