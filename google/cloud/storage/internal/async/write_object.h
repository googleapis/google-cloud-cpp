// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_OBJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_OBJECT_H

#include "google/cloud/storage/internal/storage_stub.h"
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

/**
 * Performs a single attempt to open a bidi-streaming write RPC.
 *
 * Before we can use a bidi-streaming write RPC we must call `Start()`, and
 * then call `Read()` to check the RPC start was successful.
 *
 * Using C++20 coroutines we would write this as:
 *
 * @code
 * using StreamingRpc = google::cloud::AsyncStreamingReadWriteRpc<
 *     google::storage::v2::BidiWriteObjectRequest,
 *     google::storage::v2::BidiWriteObjectResponse>;
 *
 *
 * future<StatusOr<google::storage::v2::BidiWriteObjectResponse>> Call(
 *     StreamingRpc rpc,
 *     google::storage::v2::BidiWriteObjectRequest request) {
 *   auto start = co_await rpc->Start();
 *   if (!start) co_return co_await rpc->Finish();
 *   auto read = co_await rpc->Read();
 *   if (!read) co_return co_await rpc->Finish();
 *   co_return std::move(*read);
 * }
 * @endcode
 *
 * As usual, all `co_await` calls become a callback. And all `co_return` calls
 * must set the value in an explicit `google::cloud::promise<>` object.
 */
class WriteObject : public std::enable_shared_from_this<WriteObject> {
 public:
  using StreamingRpc = google::cloud::AsyncStreamingReadWriteRpc<
      google::storage::v2::BidiWriteObjectRequest,
      google::storage::v2::BidiWriteObjectResponse>;

  using ReturnType = google::storage::v2::BidiWriteObjectResponse;
  /// Create a coroutine to create a bidi streaming write RPC.
  WriteObject(std::unique_ptr<StreamingRpc> rpc,
              google::storage::v2::BidiWriteObjectRequest request);

  struct WriteResult {
    std::unique_ptr<StreamingRpc> stream;
    google::storage::v2::BidiWriteObjectResponse first_response;
  };

  /// Start the coroutine.
  future<StatusOr<WriteResult>> Call();

 private:
  std::weak_ptr<WriteObject> WeakFromThis();

  void OnStart(bool ok);
  void OnWrite(bool ok);
  void OnRead(
      absl::optional<google::storage::v2::BidiWriteObjectResponse> response);
  void DoFinish();
  void OnFinish(Status status);

  std::unique_ptr<StreamingRpc> rpc_;
  promise<StatusOr<WriteResult>> promise_;
  google::storage::v2::BidiWriteObjectRequest initial_request_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_WRITE_OBJECT_H
