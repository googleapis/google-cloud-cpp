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

#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Represents the response from reading a subset of an object.
struct ReadObjectRangeResponse {
  /**
   * The final status of the download.
   *
   * Downloads can have partial failures, where only a subset of the data is
   * successfully downloaded, and then the connection is interrupted. With the
   * default configuration, the client library resumes the download. If,
   * however, the `storage::RetryPolicy` is exhausted, only the partial results
   * are returned, and the last error status is returned here.
   */
  Status status;

  /// If available, the full object metadata.
  absl::optional<storage::ObjectMetadata> object_metadata;

  /**
   * The object contents.
   *
   * The library receives the object contents as a sequence of `std::string`.
   * To avoid copies the library returns the sequence to the application. If
   * you need to consolidate the contents use something like:
   *
   * @code
   * ReadObjectRangeResponse response = ...;
   * auto all = std::accumulate(
   *     response.contents.begin(), response.contents.end(), std::string{},
   *     [](auto a, auto b) { a += b; return a; });
   * @endcode
   */
  std::vector<std::string> contents;

  /**
   * Per-request metadata and annotations.
   *
   * These are intended as debugging tools, they are subject to change without
   * notice.
   */
  std::multimap<std::string, std::string> request_metadata;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_OBJECT_RESPONSES_H
