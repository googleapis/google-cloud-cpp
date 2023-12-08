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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_READER_H

#include "google/cloud/storage/async/object_responses.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/async/token.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A handle for streaming downloads.
 *
 * Applications use this object to handle asynchronous streaming downloads. The
 * application repeatedly calls `Read()` until it has received all the data it
 * wants.
 */
class AsyncReader {
 public:
  /// Creates a reader that always returns errors.
  AsyncReader() = default;

  /// Initialize a reader from its implementation class.
  explicit AsyncReader(std::unique_ptr<AsyncReaderConnection> impl)
      : impl_(std::move(impl)) {}

  ///@{
  /// @name Move-only.
  AsyncReader(AsyncReader&&) noexcept = default;
  AsyncReader& operator=(AsyncReader&&) noexcept = default;
  AsyncReader(AsyncReader const&) = delete;
  AsyncReader& operator=(AsyncReader const&) = delete;
  ///@}

  /**
   * Destructor.
   *
   * If the download has not completed, the destructor cancels the underlying
   * `AsyncReaderConnection` and discards any remaining data in the background.
   *
   * The destructor returns as soon as this background task is scheduled. It
   * does **not** block waiting for the download to cancel. This may delay the
   * termination of the associated completion queue.
   */
  ~AsyncReader();

  /**
   * Retrieves more data from the object.
   *
   * The returned future becomes satisfied when more data is available. Reading
   * an object can fail even after the download starts, thus this function
   * returns wraps the value with `StatusOr<>`. A successful end-of-stream is
   * indicated by an invalid token.
   *
   * @par Example (with coroutines)
   * @code
   * future<std::int64_t> CountLines(AsyncReader reader, AsyncToken token) {
   *   std::int64_t result = 0;
   *   while (token.valid()) {
   *     // Wait for next chunk and throw on error.
   *     auto [payload, t_] = (co_await reader.Read(std::move(token))).value();
   *     token = std::move(t_);
   *     for (absl::string_view c : payload.contents()) {
   *       result += std::count_if(c.begin(), c.end(), '\n');
   *     }
   *   }
   *   co_return result;
   * }
   * @endcode
   */
  future<StatusOr<std::pair<ReadPayload, AsyncToken>>> Read(AsyncToken);

  /// Returns request metadata for troubleshooting / debugging purposes.
  RpcMetadata GetRequestMetadata();

 private:
  std::unique_ptr<AsyncReaderConnection> impl_;
  bool finished_ = false;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_READER_H
