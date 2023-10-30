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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_CONNECTION_H

#include "google/cloud/storage/async_object_requests.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An interface to asynchronously perform resumable uploads.
 *
 * The client library uses the
 * `google.storage.v2.StorageService.BidiWriteObject` RPC to perform
 * asynchronous resumable uploads to Google Cloud Storage. As the name implies,
 * this is a bi-directional RPC. The `Write()` messages upload the object data.
 * The `Read()` messages include information about how much of the uploaded data
 * has been persisted. Only `Write()` messages that explicitly request a 'flush'
 * or finalize the upload trigger a `Read()` message.
 *
 * This interface can be used to mock the behavior of these bidirectional
 * streaming RPCs. Application may use these mocks in their own tests.
 *
 * @warning
 * @parblock
 * We expect most applications will use this class in mocks or via the
 * `AsyncWriter` wrapper, and do not recommend its use outside mocks.
 *
 * If using this class directly keep in mind the following restrictions:
 *
 * - Applications must never destroy an `AsyncWriterConnection` object while any
 *   calls to `Write()` or `Finalize()` are pending.
 * - Applications must have at most one call to `Write()` pending.
 * - Applications must not issue any `Finalize()` calls while a `Write()` call
 *   is pending.
 * - Applications must only issue one `Finalize()` call.
 * @endparblock
 *
 */
class AsyncWriterConnection {
 public:
  virtual ~AsyncWriterConnection() = default;

  /// Cancels the streaming RPC, terminating any pending operations.
  virtual void Cancel() = 0;

  /// Returns the upload id. Used to checkpoint the state and resume uploads.
  virtual std::string upload_id() const = 0;

  /// Returns the last known state of the upload. Updated by `Query()` and
  /// during initialization.
  virtual absl::variant<std::int64_t, storage::ObjectMetadata> persisted_state()
      const = 0;

  /// Uploads some data to the service.
  virtual future<Status> Write(WritePayload payload) = 0;

  /// Finalizes an upload.
  virtual future<StatusOr<storage::ObjectMetadata>> Finalize(WritePayload) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_WRITER_CONNECTION_H
