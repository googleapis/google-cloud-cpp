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

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/version.h"
#include <google/storage/v2/storage.pb.h>
#include <chrono>
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
  google::cloud::internal::StreamingRpcMetadata metadata;
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_ACCUMULATE_READ_OBJECT_H
