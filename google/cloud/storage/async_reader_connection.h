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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_READER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_READER_CONNECTION_H

#include "google/cloud/storage/async_object_responses.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The `*Connection` object for `AsyncReader`.
 *
 * Applications should have little need to use this class directly. They should
 * use `AsyncReader` instead, which provides an easier to use interface.
 *
 * In tests, this class can be used to mock the behavior of `AsyncReader`.
 */
class AsyncReaderConnection {
 public:
  /// The value returned by `Read()`, see the function for more details.
  using ReadResponse = absl::variant<ReadPayload, Status>;

  virtual ~AsyncReaderConnection() = default;

  /**
   * Cancels the current download.
   *
   * Callers should continue reading until `Read()` is satisfied with a
   * `Status`.
   */
  virtual void Cancel() = 0;

  /**
   * Asks for more data.
   *
   * Retrieving more data can result in three outcomes:
   * - Additional data (a `ReadPayload`) is available.
   * - The download is interrupted with an error.
   * - The download has completed successfully.
   *
   * A `StatusOr<>` cannot represent the last outcome, we need an
   * `absl::variant<>` in this case.  We could have used
   * `StatusOr<absl::optional<ReadPayload>>` but that sounds unnecessarily
   * complicated.
   */
  virtual future<ReadResponse> Read() = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_READER_CONNECTION_H
