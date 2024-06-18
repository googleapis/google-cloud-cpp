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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_OBJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_OBJECT_H

#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Computes the `x-goog-request-params` for a bidi streaming read.
std::string RequestParams(
    google::storage::v2::BidiReadObjectRequest const& request);

/**
 * Performs a single attempt to open a bidi-streaming read RPC.
 *
 * Before we can use a bidi-streaming read RPC we must call `Start()`, and
 * send the initial message via `Write()`.
 *
 * Using C++20 coroutines we would write this as:
 *
 * @code
 * using StreamingRpc = google::cloud::AsyncStreamingReadWriteRpc<
 *     google::storage::v2::BidiReadObjectRequest,
 *     google::storage::v2::BidiReadObjectResponse>;
 *
 * struct Response {
 *   std::shared_ptr<StreamingRpc> rpc;
 *   google::storage::v2::Object metadata;
 *   google::storage::v2::BidiReadHandle handle;
 * };
 *
 * future<StatusOr<std::unique_ptr<StreamingRpc>>> Open(
 *     storage_internal::StorageStub& stub,
 *     CompletionQueue& cq,
 *     std::shared_ptr<grpc::ClientContext> context,
 *     google::cloud::internal::ImmutableOptions options,
 *     google::storage::v2::BidiReadObjectRequest request) {
 *   context->AddMetadata("x-goog-request-params", RequestParams(request));
 *   auto rpc = stub->AsyncBidiReadObject(
 *       cq, std::move(context), std::move(options));
 *   auto start = co_await rpc->Start();
 *   if (!start) co_return co_await rpc->Finish();
 *   auto write = co_await rpc->Write(request, grpc::WriteOptions{});
 *   if (!write) co_return co_await rpc->Finish();
 *   co_return rpc;
 * }
 * @endcode
 *
 * As usual, all `co_await` calls because a callback. And all `co_return` calls
 * must set the value in an explicit `google::cloud::promise<>` object.
 */
class OpenObject : public std::enable_shared_from_this<OpenObject> {
 public:
  using StreamingRpc = google::cloud::AsyncStreamingReadWriteRpc<
      google::storage::v2::BidiReadObjectRequest,
      google::storage::v2::BidiReadObjectResponse>;

  /// Create a coroutine to create an open a bidi streaming read RPC.
  OpenObject(storage_internal::StorageStub& stub, CompletionQueue& cq,
             std::shared_ptr<grpc::ClientContext> context,
             google::cloud::internal::ImmutableOptions options,
             google::storage::v2::BidiReadObjectRequest request);

  /// Start the coroutine.
  future<StatusOr<std::shared_ptr<StreamingRpc>>> Call();

 private:
  std::weak_ptr<OpenObject> WeakFromThis();

  static std::unique_ptr<StreamingRpc> CreateRpc(
      storage_internal::StorageStub& stub, CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::cloud::internal::ImmutableOptions options,
      google::storage::v2::BidiReadObjectRequest const& request);

  void OnStart(bool ok);
  void OnWrite(bool ok);
  void DoFinish();
  void OnFinish(Status status);

  std::shared_ptr<StreamingRpc> rpc_;
  promise<StatusOr<std::shared_ptr<StreamingRpc>>> promise_;
  google::storage::v2::BidiReadObjectRequest initial_request_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPEN_OBJECT_H
