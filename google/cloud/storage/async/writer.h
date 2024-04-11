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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_H

#include "google/cloud/storage/async/object_requests.h"
#include "google/cloud/storage/async/token.h"
#include "google/cloud/storage/async/writer_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <google/storage/v2/storage.pb.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Perform resumable uploads asynchronously.
 *
 * Resumable uploads allow applications to continue uploading data after network
 * disconnects and application restarts. To resume an upload the library first
 * queries the current state of the upload. The upload uses this information to
 * send the remaining data. Applications only need to checkpoint a string, the
 * `UploadId()`, to resume an upload even after the application itself restarts.
 *
 * Some data sources do not permit rewinding to an arbitrary point. For example:
 * if the application is receiving streaming data from an external source it may
 * be impossible to shutdown the application and recover the data streamed while
 * the application was down.
 *
 * This API does not support resuming uploading data from streaming data
 * sources. If the upload is interrupted you must be able to start sending data
 * from an arbitrary point.
 */
class AsyncWriter {
 public:
  AsyncWriter() = default;
  explicit AsyncWriter(std::unique_ptr<AsyncWriterConnection> impl);
  ~AsyncWriter();

  ///@{
  /// @name Move-only.
  AsyncWriter(AsyncWriter&&) noexcept = default;
  AsyncWriter& operator=(AsyncWriter&&) noexcept = default;
  AsyncWriter(AsyncWriter const&) = delete;
  AsyncWriter& operator=(AsyncWriter const&) = delete;
  ///@}

  /**
   * The upload id.
   *
   * Applications that need to resume uploads after a restart should checkpoint
   * this value to persistent storage.
   *
   * @note
   * Calling this function on a default-constructed or moved-from `AsyncWriter`
   * results in undefined behavior.
   */
  std::string UploadId() const;

  /**
   * The state of the current upload.
   *
   * This returns the last "known" state of the upload. The values are updated
   * (1) when the `AsyncWriter` object is created, and (2) on calls to `Query()`
   * and `Finalize()`.
   *
   * Applications may finalize an upload, and then try to resume the upload. In
   * this case this function returns `google::storage::v2::Object`.
   *
   * During an upload the service will periodically persist the uploaded data.
   * Applications should not assume that all data "sent" is persisted. The
   * service may buffer the data in ephemeral storage before persisting it.
   *
   * If the application needs to resume an upload that was interrupted, then it
   * must check `PersistedState()` before sending data. The service will assume
   * that new data starts at the correct offset.
   *
   * If an upload is resumed after it is finalized, the library will return a
   * variant holding `google::storage::v2::Object` value.
   *
   * Otherwise the variant returns the size of the persisted data. The
   * application should send the remaining data to upload, starting from this
   * point.
   *
   * @note
   * Calling this function on a default-constructed or moved-from `AsyncWriter`
   * results in undefined behavior.
   */
  absl::variant<std::int64_t, google::storage::v2::Object> PersistedState()
      const;

  /// Upload @p payload returning a new token to continue the upload.
  future<StatusOr<AsyncToken>> Write(AsyncToken token, WritePayload payload);

  /// Finalize the upload with the existing data.
  future<StatusOr<google::storage::v2::Object>> Finalize(AsyncToken token);

  /**
   * Upload @p payload and then finalize the upload.
   */
  future<StatusOr<google::storage::v2::Object>> Finalize(AsyncToken token,
                                                         WritePayload payload);

  /**
   * The headers (if any) returned by the service. For debugging only.
   *
   * @warning The contents of these headers may change without notice. Unless
   *     documented in the API, headers may be removed or added by the service.
   *     Furthermore, the headers may change from one version of the library to
   *     the next, as we find more (or different) opportunities for
   *     optimization.
   */
  RpcMetadata GetRequestMetadata() const;

 private:
  std::shared_ptr<AsyncWriterConnection> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_H
