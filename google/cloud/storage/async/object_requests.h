// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_REQUESTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_REQUESTS_H

#include "google/cloud/storage/internal/async/connection_fwd.h"
#include "google/cloud/storage/internal/async/write_payload_fwd.h"
#include "google/cloud/storage/internal/grpc/make_cord.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/version.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include <cstdint>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An opaque representation of the data for an object payload.
 */
class WritePayload {
 public:
  /// Creates an empty payload.
  WritePayload() = default;

  /// Creates a payload from @p p.
  explicit WritePayload(std::string p)
      : impl_(storage_internal::MakeCord(std::move(p))) {}

  /**
   * Creates a payload from @p p.
   *
   * @tparam T must be either:
   * - `char`, or
   * - `std::byte`, or
   * - an integer type that is *exactly* 8-bits wide (such as `std::int8_t`), or
   * - `std::string`, or
   * - `std::vector<U>`, where `U` is `char`, `std::byte`, or or an integer type
   *   that is exactly 8-bits wide
   *
   * @param p the resulting object takes ownership of the data in @p p.
   */
  template <typename T>
  explicit WritePayload(std::vector<T> p)
      : impl_(storage_internal::MakeCord(std::move(p))) {}

  /// Returns true if the payload has no data.
  bool empty() const { return impl_.empty(); }

  /// Returns the total size of the data.
  std::size_t size() const { return impl_.size(); }

  /**
   * Returns views into the data.
   *
   * Note that changing `*this` in any way (assignment, destruction, etc.)
   * invalidates all the returned buffers.
   */
  std::vector<absl::string_view> payload() const {
    return {impl_.chunk_begin(), impl_.chunk_end()};
  }

 private:
  friend struct storage_internal::WritePayloadImpl;
  explicit WritePayload(absl::Cord impl) : impl_(std::move(impl)) {}

  absl::Cord impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_REQUESTS_H
