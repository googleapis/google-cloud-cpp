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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_PARTIAL_UPLOAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_PARTIAL_UPLOAD_H

#include "google/cloud/storage/internal/hash_function.h"
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/strings/cord.h"
#include "google/storage/v2/storage.pb.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Performs a single attempt to upload data for an object.
 *
 * This is a building block in the resumable uploads implementation. The GCS
 * upload protocol only accepts messages up to 2 MiB, but applications may
 * provide much larger buffers.  We need to break the data into multiple
 * asynchronous calls.
 *
 * This implements a coroutine to upload part of an object. It is used in the
 * implementation of `AsyncClient::WriteObject()`. The payload for the object
 * is represented by a single `absl::Cord`.
 *
 * Using C++20 coroutines we would write this as:
 *
 * @code
 * future<StatusOr<bool>> PartialUpload(
 *     std::unique_ptr<storage::internal::HashFunction> hash_function,
 *     std::shared_ptr<StreamingBidirWriteRpc> rpc,
 *     google::storage::v2::BidiWriteObjectRequest request,
 *     absl::Cord data, LastMessageAction action) {
 *   auto constexpr kMax = static_cast<std::size_t>(
 *       google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES);
 *   do {
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
 *     if (last_message) {
 *       wopt.set_last_message();
 *       if (action == kFlush) request.set_flush(true);
 *       if (action == kFinalize) {
 *         // This is the last block, compute full checksums and set flags.
 *         auto status = Finalize(request, wopt, *hash_function);
 *         if (!status.ok()) {
 *           rpc->Cancel();
 *           co_return status;
 *         }
 *       }
 *     }
 *     // Write the data, breaking out of the loop on error.
 *     if (!co_await rpc->Write(request, wopt)) co_return false;
 *     // OnWrite()
 *     request.clear_first_message();
 *     request.set_offset(request.offset() + n);
 *   } while (!data.empty());
 *   co_return true;
 * }
 * @endcode
 */
class PartialUpload : public std::enable_shared_from_this<PartialUpload> {
 public:
  using StreamingWriteRpc = google::cloud::AsyncStreamingReadWriteRpc<
      google::storage::v2::BidiWriteObjectRequest,
      google::storage::v2::BidiWriteObjectResponse>;

  enum LastMessageAction { kNone, kFlush, kFinalize, kFinalizeWithChecksum };

  static std::shared_ptr<PartialUpload> Call(
      std::shared_ptr<StreamingWriteRpc> rpc,
      std::shared_ptr<storage::internal::HashFunction> hash_function,
      google::storage::v2::BidiWriteObjectRequest request, absl::Cord data,
      LastMessageAction action) {
    return std::shared_ptr<PartialUpload>(
        new PartialUpload(std::move(rpc), std::move(hash_function),
                          std::move(request), std::move(data), action));
  }

  future<StatusOr<bool>> Start();

 private:
  PartialUpload(std::shared_ptr<StreamingWriteRpc> rpc,
                std::shared_ptr<storage::internal::HashFunction> hash_function,
                google::storage::v2::BidiWriteObjectRequest request,
                absl::Cord data, LastMessageAction action)
      : rpc_(std::move(rpc)),
        hash_function_(std::move(hash_function)),
        request_(std::move(request)),
        data_(std::move(data)),
        action_(action) {}

  void Write();
  void OnWrite(std::size_t n, bool ok);
  void WriteError(StatusOr<bool> result);

  std::weak_ptr<PartialUpload> WeakFromThis() { return shared_from_this(); }

  std::shared_ptr<StreamingWriteRpc> rpc_;
  std::shared_ptr<storage::internal::HashFunction> hash_function_;
  google::storage::v2::BidiWriteObjectRequest request_;
  absl::Cord data_;
  LastMessageAction action_;
  promise<StatusOr<bool>> result_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_PARTIAL_UPLOAD_H
