// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_REWRITER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_REWRITER_H

#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/async/rewriter_connection.h"
#include "google/cloud/storage/async/token.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <google/storage/v2/storage.pb.h>
#include <cstdint>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Perform object rewrites asynchronously.
 *
 * Object rewrites allow applications to copy objects within Google Cloud
 * Storage without having to download the data. In many cases the copy is a pure
 * metadata operation, see [Object: rewrite] for mode information.
 *
 * [Object: rewrite]:
 * https://cloud.google.com/storage/docs/json_api/v1/objects/rewrite
 */
class AsyncRewriter {
 public:
  AsyncRewriter() = default;
  explicit AsyncRewriter(std::shared_ptr<AsyncRewriterConnection> impl);
  ~AsyncRewriter();

  ///@{
  /// @name Move-only.
  AsyncRewriter(AsyncRewriter&&) noexcept = default;
  AsyncRewriter& operator=(AsyncRewriter&&) noexcept = default;
  AsyncRewriter(AsyncRewriter const&) = delete;
  AsyncRewriter& operator=(AsyncRewriter const&) = delete;
  ///@}

  /**
   * Run one more iteration of the rewrite process.
   *
   * Applications may checkpoint the rewrite token and use it to resume rewrites
   * after restarting.
   *
   * @note
   * Calling this function on a default-constructed or moved-from
   * `AsyncRewriter` results in undefined behavior.
   */
  future<StatusOr<std::pair<google::storage::v2::RewriteResponse, AsyncToken>>>
  Iterate(AsyncToken token);

 private:
  std::shared_ptr<AsyncRewriterConnection> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_REWRITER_H
