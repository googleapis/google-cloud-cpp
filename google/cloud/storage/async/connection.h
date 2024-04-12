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

#include "google/cloud/storage/async/object_responses.h"
#include "google/cloud/storage/async/write_payload.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class AsyncReaderConnection;
class AsyncRewriterConnection;
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

  /// The options used to configure this connection, with any defaults applied.
  virtual Options options() const = 0;

  /**
   * A thin wrapper around the `InsertObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct InsertObjectParams {
    /// The bucket and object name for the new object. Includes any optional
    /// parameters, such as pre-conditions on the insert operation, or metadata
    /// attributes.
    google::storage::v2::WriteObjectRequest request;
    /// The bulk payload, sometimes called the "media" or "contents".
    WritePayload payload;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Insert a new object.
  virtual future<StatusOr<google::storage::v2::Object>> InsertObject(
      InsertObjectParams p) = 0;

  /**
   * A thin wrapper around the `ReadObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct ReadObjectParams {
    /// The name of the bucket and object to read. Includes optional parameters,
    /// such as pre-conditions on the read operation, or the range within the
    /// object to read.
    google::storage::v2::ReadObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Asynchronously create a stream to read object contents.
  virtual future<StatusOr<std::unique_ptr<AsyncReaderConnection>>> ReadObject(
      ReadObjectParams p) = 0;

  /// Read a range from an object returning all the contents.
  virtual future<StatusOr<ReadPayload>> ReadObjectRange(ReadObjectParams p) = 0;

  /**
   * A thin wrapper around the `WriteObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct UploadParams {
    /// The bucket name and object name for the new object. Includes optional
    /// parameters such as pre-conditions on the new object.
    google::storage::v2::StartResumableWriteRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Start (or resume) an upload configured for persistent sources.
  virtual future<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartUnbufferedUpload(UploadParams p) = 0;

  /// Start (or resume) an upload configured for streaming sources.
  virtual future<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartBufferedUpload(UploadParams p) = 0;

  /**
   * A thin wrapper around the `QueryWriteStatus()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct ResumeUploadParams {
    /// The upload id and any common object request parameters. Note that
    /// bucket name, object name, and pre-conditions are saved as part of the
    /// service internal information about the upload id.
    google::storage::v2::QueryWriteStatusRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Resume an upload configured for persistent sources.
  virtual future<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeUnbufferedUpload(ResumeUploadParams p) = 0;

  /// Resume an upload configured for streaming sources.
  virtual future<
      StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeBufferedUpload(ResumeUploadParams p) = 0;

  /**
   * A thin wrapper around the `ComposeObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct ComposeObjectParams {
    /// The bucket name, the name of the source objects, and the name of the
    /// destination object. Including pre-conditions on the source objects, the
    /// destination object, and other optional parameters.
    google::storage::v2::ComposeObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Create a new object by composing (concatenating) the contents of existing
  /// objects.
  virtual future<StatusOr<google::storage::v2::Object>> ComposeObject(
      ComposeObjectParams p) = 0;

  /**
   * A thin wrapper around the `DeleteObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct DeleteObjectParams {
    /// The bucket and object name for the object to be deleted. Including
    /// pre-conditions on the object and other optional parameters.
    google::storage::v2::DeleteObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Delete an object.
  virtual future<Status> DeleteObject(DeleteObjectParams p) = 0;

  /**
   * A thin wrapper around the `StartRewriteObject()` parameters.
   *
   * We use a single struct as the input parameter for this function to
   * prevent breaking any mocks when additional parameters are needed.
   */
  struct RewriteObjectParams {
    /// The source and destination bucket and object names. Including
    /// pre-conditions on the object and other optional parameters.
    google::storage::v2::RewriteObjectRequest request;
    /// Any options modifying the RPC behavior, including per-client and
    /// per-connection options.
    Options options;
  };

  /// Start an object rewrite.
  virtual std::shared_ptr<AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_CONNECTION_H
