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

#include "google/cloud/storage/async_object_requests.h"
#include "google/cloud/storage/async_token.h"
#include "google/cloud/storage/async_writer_connection.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Perform resumable uploads asynchronously.
 *
 * Resumable uploads allow applications to continue uploading data even after
 * disconnects and even after application restarts. To resume an upload the
 * library first queries the current state of the upload, the upload uses this
 * information to send the remaining data. Applications only need to checkpoint
 * a string, the `UploadId()`, to resume an upload even after the application
 * itself restarts.
 *
 * Some data sources do not permit rewinding to an arbitrary point, for example:
 * if the application is receiving streaming data from an external sourcein it
 * may be impossible to shutdown the application and recover the data streamed
 * while the application was down.
 *
 * Nevertheless, even with streaming data sources, resumable uploads can be used
 * to recover from most network problems between the application and Google
 * Cloud Storage. The application can buffer any streaming data in memory, and
 * periodically force a flush in the resumable upload. The service will return
 * the number of bytes persisted and the application and then discard the
 * corresponding bytes from its buffer.
 *
 * Some caveats apply:
 * - The application should be prepared for "partial flush" results, that is
 *   service may indicate than fewer bytes were persisted vs. the number of
 *   bytes sent.
 * - The application can continue to upload data after a flush. It is possible
 *   that a flush request returns more persisted bytes than were available when
 *   the flush request is sent.
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
   * this case this function returns `storage::ObjectMetadata`.
   *
   * More commonly, applications resume an upload that is in progress, or
   * periodically flush the
   *
   * During an upload the service will periodically persist the uploaded data.
   * Applications should not assume that all data "sent" is persisted. The
   * service may buffer the data in ephemeral storage before persisting it.
   *
   * If the application needs to resume an upload that was interrupted, then it
   * must check `persisted_state()` before sending data. The service will assume
   * that new data starts at the correct offset.
   *
   * If an upload is resumed after it is finalized the library will return a
   * variant holding `storage::ObjectMetadata` value.
   *
   * Otherwise the variant returns the size of the persisted data. The
   * application should send the remaining data to upload, starting from this
   * point.
   */
  absl::variant<std::int64_t, storage::ObjectMetadata> PersistedState() const;

  /**
   * Returns the offset for the next `Write()` or `Finalize()` call.
   *
   * This is automatically updated once the `Write()` call is satisfied.
   * Applications may use this information to seek the right data from the
   * source.
   */
  absl::optional<std::int64_t> current_offset() const;

  /// Upload @p payload returning a new token to continue the upload.
  future<StatusOr<AsyncToken>> Write(AsyncToken token, WritePayload payload);

  /// Finalize the upload with the existing data.
  future<StatusOr<storage::ObjectMetadata>> Finalize(AsyncToken token);

  /**
   * Upload @p payload and then finalize the upload.
   */
  future<StatusOr<storage::ObjectMetadata>> Finalize(AsyncToken token,
                                                     WritePayload payload);

 private:
  std::shared_ptr<AsyncWriterConnection> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_H
