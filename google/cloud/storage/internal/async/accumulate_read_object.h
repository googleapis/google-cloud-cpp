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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H

#include "google/cloud/storage/async/object_responses.h"
#include "google/cloud/storage/internal/storage_stub.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/rpc_metadata.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <chrono>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Accumulate the responses from one (or many) `AsyncReadObject()` calls.
 *
 * The asynchronous APIs to read objects will always be "ranged", with the
 * application setting the maximum number of bytes. It simplifies the
 * implementation to first collect all the data into this struct, and then
 * manipulate it into something more idiomatic, e.g., something where the object
 * metadata is already parsed, and the checksums already validated.
 */
struct AsyncAccumulateReadObjectResult {
  std::vector<google::storage::v2::ReadObjectResponse> payload;
  google::cloud::RpcMetadata metadata;
  Status status;
};

/**
 * Accumulate the result of a single `AsyncReadObject()` call.
 *
 * This function (asynchronously) consumes all the results from @p stream and
 * returns them in a single result.  The @p timeout parameter can be used to
 * abort the download for lack of progress, i.e., it applies to each `Read()`
 * call, not to the full download.
 *
 * In C++20 with coroutines, a simplified implementation would be:
 *
 * @code
 * future<AsyncAccumulateReadObjectResult> AsyncAccumulateReadObjectPartial(
 *   CompletionQueue cq, ... stream, std::chrono::milliseconds timeout) {
 *   AsyncAccumulateReadObjectResult result;
 *   auto start = co_await stream->Start();
 *   while (start) {
 *     auto read = co_await stream->Read();
 *     if (!read.has_value()) break;
 *     result.payload.push_back(*std::move(read));
 *   }
 *   result.status = co_await stream->Finish();
 *   result.metadata = stream->GetRequestMetadata();
 *   co_return result;
 * }
 * @encode
 */
future<AsyncAccumulateReadObjectResult> AsyncAccumulateReadObjectPartial(
    CompletionQueue cq,
    std::unique_ptr<google::cloud::internal::AsyncStreamingReadRpc<
        google::storage::v2::ReadObjectResponse>>
        stream,
    std::chrono::milliseconds timeout);

/**
 * Accumulates the results of `AsyncReadObject()`, using a retry loop if needed.
 *
 * The implementation of `AsyncClient::ReadObject()` needs to accumulate the
 * results of one or more `ReadObject()` requests (which are streaming read
 * RPCs) and return a single `future<T>` to the application. The implementation
 * must also automatically resume interrupted calls, and restart the download
 * from the last received byte.
 *
 * If we were using C++20, this would be a coroutine, and we will use that
 * coroutine to explain what this code does.  Essentially this is a retry
 * loop, where we advance the read_offset() after each retry.
 *
 * The preamble should be self-explanatory:
 *
 * @code
 * using ::google::storage::v2::ReadResponse;
 * using ::google::storage::v2::ReadRequest;
 *
 * future<AsyncAccumulateReadObject::Result> AsyncAccumulateReadObjectFull(
 *     CompletionQueue cq,
 *     std::shared_ptr<StorageStub> stub,
 *     std::function<std::shared_ptr<grpc::ClientContext>()> context_factory,
 *     google::storage::v2::ReadObjectRequest request,
 *     std::chrono::milliseconds timeout,
 *     google::cloud::internal::ImmutableOptions options) {
 *   auto retry = options->get<storage::RetryPolicyOption>()->clone();
 *   auto backoff = options->get<storage::BackoffPolicyOption>()->clone();
 *   // We will use a variable of the coroutine to accumulate the (partial)
 *   // reads.
 *   AsyncAccumulateReadObjectResult result;
 *   // We initialize it with an error status to handle the (fairly unlikely)
 *   // case where a new retry policy starts exhausted:
 *   result.status = Status(StatusCode::kDeadlineExceeded,
 *                          "retry policy exhausted before first request");
 *   while (!retry->IsExhausted()) {
 *     // Perform a partial read and (asynchronously) accumulate the results:
 *     auto stream = stub->AsyncReadObject(
 *         cq, context_factory(), request);
 *     auto partial = co_await AsyncAccumulateReadObject::Start(
 *         cq, std::move(stream), timeout);
 *
 *     // We need to know how much data was received to update the read offset:
 *     auto const size = std::accumulate(
 *         partial.payload.begin(), partial.payload.end(), std::size_t{0},
 *         [](std::size_t a, google::storage::v2::ReadObjectResponse const& r) {
 *           if (!r.has_checksummed_data()) return a;
 *           return a + r.checksummed_data().content().size();
 *         });
 *     // We accumulate the partial result into the full result:
 *     result.status = std::move(partial.status);
 *     result.payload.insert(result.payload.end(),
 *                           std::make_move_iterator(partial.payload.begin()),
 *                           std::make_move_iterator(partial.payload.end()));
 *     result.metadata.insert(std::make_move_iterator(partial.metadata.begin()),
 *                            std::make_move_iterator(partial.metadata.end()));
 *     // If this receives more data than expected we need to signal some error
 *     if (size > request.read_limit()) {
 *       result.status = Status(
 *           StatusCode::kInternal, "too much data received");
 *       co_return result;
 *     }
 *     request.set_read_offset(request.read_offset() + size);
 *     request.set_read_limit(request.read_limit() - size);
 *     // If the partial read completed the request we return, otherwise we
 *     // update the retry policy and backoff:
 *     if (result.status.ok()) co_return result;
 *     if (!retry->OnFailure(result.status)) break;
 *     co_await cq.MakeRelativeTimer(backoff->OnCompletion());
 *   }
 *   co_return result;
 * }
 * @endcode
 *
 * @param cq the completion queue used to run all background operations.
 * @param stub the wrapper around the gRPC-generated stub, maybe decorated to
 *     log requests and update the context metadata.
 * @param context_factory a functor to create a `grpc::ClientContext` and maybe
 *     initialize some attributes.
 * @param request what object and range within the object to read.
 * @param options any per-call configuration. At this time, only the retry and
 *     backoff policies are used by this function.
 */
future<AsyncAccumulateReadObjectResult> AsyncAccumulateReadObjectFull(
    CompletionQueue cq, std::shared_ptr<StorageStub> stub,
    std::function<std::shared_ptr<grpc::ClientContext>()> context_factory,
    google::storage::v2::ReadObjectRequest request,
    google::cloud::internal::ImmutableOptions options);

/**
 * Convert the proto into a more stable representation.
 *
 * The `contents()` may be an `absl::Cord` or an `std::string`, depending on the
 * Protobuf version. We don't want to expose that complexity to customers.
 * Furthermore, like all APIs in `absl::`, there is no backwards compatibility
 * guarantee, so we don't want to expose customers to these (potential) breaking
 * changes.
 */
StatusOr<storage_experimental::ReadPayload> ToResponse(
    AsyncAccumulateReadObjectResult accumulated);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H
