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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_INSERT_OBJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_INSERT_OBJECT_H

#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_streaming_write_rpc.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include <google/storage/v2/storage.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Performs a single attempt to insert an object.
 *
 * This implements a coroutine to upload an object. It is used in the
 * implementation of `AsyncClient::InsertObject()`. The payload for the object
 * is represented by a single `absl::Cord`.
 *
 * Using C++20 coroutines we would write this as:
 *
 * @code
 * future<StatusOr<storage::ObjectMetadata>> AsyncObject(
 *     std::unique_ptr<storage::internal::HashFunction> hash_function,
 *     std::unique_ptr<StreamingWriteRpc> rpc,
 *     google::storage::v2::WriteObjectRequest request, absl::Cord data,
 *     google::cloud::internal::ImmutableOptions options) {
 *   auto constexpr kMax = static_cast<std::size_t>(
 *       google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
 *   auto rpc_ok = co_await rpc->Start();
 *   while (rpc_ok) {
 *     auto const n = std::min(data.size(), kMax);
 *     auto next = data.Subcord(0, n);
 *     data.RemovePrefix(n);
 *     auto crc32c = Crc32c(next);
 *     hash_function.Update(request.offset(), next, crc32c);
 *     auto& data = *request.mutable_checksummed_data();
 *     data.set_content(std::move(next));
 *     data.set_crc32c(crc32c);
 *     auto wopt = grpc::WriteOptions{};
 *     auto const last_message = data_.empty();
 *     request.set_finish_write(last_message);
 *     if (last_message) {
 *       // This is the last block, compute full checksums and set flags.
 *       auto status = Finalize(request, wopt, *hash_function);
 *       if (!status.ok()) {
 *         rpc->Cancel();
 *         (void)co_await rpc->Finish();
 *         co_return status;
 *       }
 *     }
 *     // Write the data, breaking out of the loop on error.
 *     rpc_ok = co_await rpc->Write(request, wopt);
 *     // We need at least one empty Write() for empty objects. Only then we
 *     // can exit the loop.
 *     if (data.empty()) break;
 *     request.clear_first_message();
 *     request.set_offset(request.offset() + n);
 *   }
 *   auto response = co_await rpc->Finish();
 *   if (!response) co_return std::move(response).status();
 *   co_return FromProto(*response, options); // convert to gcs::ObjectMetadata
 * }
 * @endcode
 */
class InsertObject : public std::enable_shared_from_this<InsertObject> {
 public:
  using StreamingWriteRpc = google::cloud::internal::AsyncStreamingWriteRpc<
      google::storage::v2::WriteObjectRequest,
      google::storage::v2::WriteObjectResponse>;

  static std::shared_ptr<InsertObject> Call(
      std::unique_ptr<StreamingWriteRpc> rpc,
      std::unique_ptr<storage::internal::HashFunction> hash_function,
      google::storage::v2::WriteObjectRequest request, absl::Cord data,
      google::cloud::internal::ImmutableOptions options) {
    return std::shared_ptr<InsertObject>(new InsertObject(
        std::move(rpc), std::move(hash_function), std::move(request),
        std::move(data), std::move(options)));
  }

  future<StatusOr<google::storage::v2::Object>> Start();

 private:
  InsertObject(std::unique_ptr<StreamingWriteRpc> rpc,
               std::unique_ptr<storage::internal::HashFunction> hash_function,
               google::storage::v2::WriteObjectRequest request, absl::Cord data,
               google::cloud::internal::ImmutableOptions options)
      : rpc_(std::move(rpc)),
        hash_function_(std::move(hash_function)),
        request_(std::move(request)),
        data_(std::move(data)),
        options_(std::move(options)) {}

  void OnStart(bool ok);
  void Write();
  void OnError(Status status);
  void OnWrite(std::size_t n, bool ok);
  void OnFinish(StatusOr<google::storage::v2::WriteObjectResponse> response);

  std::weak_ptr<InsertObject> WeakFromThis() { return shared_from_this(); }

  std::unique_ptr<StreamingWriteRpc> rpc_;
  std::unique_ptr<storage::internal::HashFunction> hash_function_;
  google::storage::v2::WriteObjectRequest request_;
  absl::Cord data_;
  google::cloud::internal::ImmutableOptions options_;
  promise<StatusOr<google::storage::v2::Object>> result_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_INSERT_OBJECT_H
