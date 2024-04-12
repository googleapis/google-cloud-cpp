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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_REWRITER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_REWRITER_CONNECTION_H

#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An interface for asynchronous object rewrite implementations.
 *
 * @warning
 * @parblock
 * We expect most applications will use this class in mocks or via the
 * `AsyncRewriter` wrapper, and do not recommend its use outside mocks.
 *
 * If using this class directly keep in mind the following restrictions:
 *
 * - Never destroy an `AsyncRewriterConnection` object while an `Iterate()`
 *   call is pending.
 * - Have at most one call to `Iterate()` pending at a time.
 * @endparblock
 */
class AsyncRewriterConnection {
 public:
  virtual ~AsyncRewriterConnection() = default;

  /// Uploads some data to the service.
  virtual future<StatusOr<google::storage::v2::RewriteResponse>> Iterate() = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_REWRITER_CONNECTION_H
