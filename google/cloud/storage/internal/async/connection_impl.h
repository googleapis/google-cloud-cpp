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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CONNECTION_IMPL_H

#include "google/cloud/storage/async/connection.h"
#include "google/cloud/storage/async/reader_connection.h"
#include "google/cloud/storage/async/rewriter_connection.h"
#include "google/cloud/storage/idempotency_policy.h"
#include "google/cloud/storage/internal/async/reader_connection_factory.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/invocation_id_generator.h"
#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <google/storage/v2/storage.pb.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class GrpcChannelRefresh;
class StorageStub;

class AsyncConnectionImpl
    : public storage_experimental::AsyncConnection,
      public std::enable_shared_from_this<AsyncConnectionImpl> {
 public:
  explicit AsyncConnectionImpl(CompletionQueue cq,
                               std::shared_ptr<GrpcChannelRefresh> refresh,
                               std::shared_ptr<StorageStub> stub,
                               Options options);
  ~AsyncConnectionImpl() override = default;

  Options options() const override { return options_; }

  future<StatusOr<google::storage::v2::Object>> InsertObject(
      InsertObjectParams p) override;

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncReaderConnection>>>
  ReadObject(ReadObjectParams p) override;

  future<StatusOr<storage_experimental::ReadPayload>> ReadObjectRange(
      ReadObjectParams p) override;

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartUnbufferedUpload(UploadParams p) override;

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartBufferedUpload(UploadParams p) override;

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeUnbufferedUpload(ResumeUploadParams p) override;

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeBufferedUpload(ResumeUploadParams p) override;

  future<StatusOr<google::storage::v2::Object>> ComposeObject(
      ComposeObjectParams p) override;

  future<Status> DeleteObject(DeleteObjectParams p) override;

  std::shared_ptr<storage_experimental::AsyncRewriterConnection> RewriteObject(
      RewriteObjectParams p) override;

  // Expose this function for testing purposes. It creates a factory to create
  // new `AsyncReaderConnection` instances at different offsets.
  AsyncReaderConnectionFactory MakeReaderConnectionFactory(
      google::cloud::internal::ImmutableOptions current,
      google::storage::v2::ReadObjectRequest request,
      std::shared_ptr<storage::internal::HashFunction> hash_function);

 private:
  std::weak_ptr<AsyncConnectionImpl> WeakFromThis() {
    return shared_from_this();
  }

  future<StatusOr<google::storage::v2::StartResumableWriteResponse>>
  StartResumableWrite(internal::ImmutableOptions current,
                      google::storage::v2::StartResumableWriteRequest request);

  future<StatusOr<google::storage::v2::QueryWriteStatusResponse>>
  QueryWriteStatus(internal::ImmutableOptions current,
                   google::storage::v2::QueryWriteStatusRequest request);

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeUpload(internal::ImmutableOptions current,
               google::storage::v2::QueryWriteStatusRequest query);

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  StartUnbufferedUploadImpl(
      internal::ImmutableOptions current,
      google::storage::v2::StartResumableWriteRequest request,
      StatusOr<google::storage::v2::StartResumableWriteResponse> response);

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  ResumeUnbufferedUploadImpl(
      internal::ImmutableOptions current,
      google::storage::v2::QueryWriteStatusRequest query,
      StatusOr<google::storage::v2::QueryWriteStatusResponse> response);

  future<StatusOr<std::unique_ptr<storage_experimental::AsyncWriterConnection>>>
  UnbufferedUploadImpl(
      internal::ImmutableOptions current,
      std::function<void(grpc::ClientContext&)> configure_context,
      google::storage::v2::BidiWriteObjectRequest request,
      std::shared_ptr<storage::internal::HashFunction> hash_function,
      std::int64_t persisted_size);

  CompletionQueue cq_;
  std::shared_ptr<GrpcChannelRefresh> refresh_;
  std::shared_ptr<StorageStub> stub_;
  Options options_;
  google::cloud::internal::InvocationIdGenerator invocation_id_generator_;
};

/// Create a connection and the default stub.
std::shared_ptr<storage_experimental::AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, Options options = Options{});

/// Create a connection with a custom stub (usually a mock).
std::shared_ptr<storage_experimental::AsyncConnection> MakeAsyncConnection(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub, Options options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_CONNECTION_IMPL_H
