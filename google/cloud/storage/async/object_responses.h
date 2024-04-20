// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_RESPONSES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_RESPONSES_H

#include "google/cloud/storage/headers_map.h"
#include "google/cloud/storage/internal/async/read_payload_fwd.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/version.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <google/storage/v2/storage.pb.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A partial or full response to an asynchronous download.
 */
class ReadPayload {
 public:
  ReadPayload() = default;

  /// Constructor from string. Applications may use this in their mocks.
  explicit ReadPayload(std::string contents);
  /// Constructor from vector of strings. Applications may use this in their
  /// mocks with more complex `contents()` results.
  explicit ReadPayload(std::vector<std::string> contents);

  /// The total size of the payload.
  std::size_t size() const { return impl_.size(); }

  /// The payload contents. These buffers are invalidated if this object is
  /// modified.
  std::vector<absl::string_view> contents() const {
    return {impl_.chunk_begin(), impl_.chunk_end()};
  }

  /// The object metadata.
  absl::optional<google::storage::v2::Object> metadata() const {
    return metadata_;
  }

  /// The starting offset of the current message.
  std::int64_t offset() const { return offset_; }

  /**
   * The headers (if any) returned by the service. For debugging only.
   *
   * @warning The contents of these headers may change without notice. Unless
   *     documented in the API, headers may be removed or added by the service.
   *     Furthermore, the headers may change from one version of the library to
   *     the next, as we find more (or different) opportunities for
   *     optimization.
   */
  storage::HeadersMap const& headers() const { return headers_; }

  ///@{
  /// Modifiers. Applications may need these in mocks.
  ReadPayload& set_metadata(google::storage::v2::Object v) & {
    metadata_ = std::move(v);
    return *this;
  }
  ReadPayload&& set_metadata(google::storage::v2::Object v) && {
    return std::move(set_metadata(std::move(v)));
  }
  ReadPayload& reset_metadata() & {
    metadata_.reset();
    return *this;
  }
  ReadPayload&& reset_metadata() && { return (std::move(reset_metadata())); }

  ReadPayload& set_headers(storage::HeadersMap v) & {
    headers_ = std::move(v);
    return *this;
  }
  ReadPayload&& set_headers(storage::HeadersMap v) && {
    return std::move(set_headers(std::move(v)));
  }
  ReadPayload& clear_headers() & {
    headers_.clear();
    return *this;
  }
  ReadPayload&& clear_headers() && {
    headers_.clear();
    return std::move(*this);
  }

  ReadPayload& set_offset(std::int64_t v) & {
    offset_ = v;
    return *this;
  }
  ReadPayload&& set_offset(std::int64_t v) && {
    return std::move(set_offset(v));
  }
  ///@}

 private:
  friend storage_internal::ReadPayloadImpl;
  explicit ReadPayload(absl::Cord impl) : impl_(std::move(impl)) {}

  absl::Cord impl_;
  std::int64_t offset_ = 0;
  absl::optional<google::storage::v2::Object> metadata_;
  storage::HeadersMap headers_;
  // The full object checksums (aka hash values), if known.
  absl::optional<storage::internal::HashValues> object_hash_values_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_RESPONSES_H
