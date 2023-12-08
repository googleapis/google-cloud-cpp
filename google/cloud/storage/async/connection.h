// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CONNECTION_H

#include "google/cloud/storage/async/object_requests.h"
#include "google/cloud/storage/async/object_responses.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class AsyncReaderConnection;
class AsyncWriterConnection;

/**
 * The `*Connection` object for `AsyncClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `AsyncClient`. This allows users to inject custom behavior (e.g.,
 * with a Google Mock object) when writing tests that use objects of type
 * `AsyncClient`.
 *
 * To create a concrete instance, see `MakeAsyncConnection()`.
 *
 * For mocking, see `storage_mocks::MockAsyncConnection`.
 */
class AsyncConnection {
 public:
  virtual ~AsyncConnection() = default;

  virtual Options options() const = 0;

  /**
   * A thin wrapper around the `InsertObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct InsertObjectParams {
    /// The metadata attributes to create the object.
    InsertObjectRequest request;
    /// The bulk payload, sometimes called the "media" or "contents".
    WritePayload payload;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Insert a new object.
  virtual future<StatusOr<storage::ObjectMetadata>> AsyncInsertObject(
      InsertObjectParams p) = 0;

  /**
   * A thin wrapper around the `ReadObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct ReadObjectParams {
    /// What object to read, what portion of the object to read, and any
    /// pre-conditions on the read.
    ReadObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Asynchronously create a stream to read object contents.
  virtual future<StatusOr<std::unique_ptr<AsyncReaderConnection>>>
  AsyncReadObject(ReadObjectParams p) = 0;

  /// Read a range from an object returning all the contents.
  virtual future<StatusOr<ReadPayload>> AsyncReadObjectRange(
      ReadObjectParams p) = 0;

  /**
   * A thin wrapper around the `WriteObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct WriteObjectParams {
    /// The metadata attributes for the new object.
    ResumableUploadRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Start (or resume) a streaming write.
  virtual future<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  AsyncWriteObject(WriteObjectParams p) = 0;

  /**
   * A thin wrapper around the `ComposeObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct ComposeObjectParams {
    /// The metadata attributes to create the object.
    ComposeObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Create a new object by composing (concatenating) the contents of existing
  /// objects.
  virtual future<StatusOr<storage::ObjectMetadata>> AsyncComposeObject(
      ComposeObjectParams p) = 0;

  /**
   * A thin wrapper around the `DeleteObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct DeleteObjectParams {
    /// The metadata attributes to create the object.
    DeleteObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Delete an object.
  virtual future<Status> AsyncDeleteObject(DeleteObjectParams p) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CONNECTION_H
