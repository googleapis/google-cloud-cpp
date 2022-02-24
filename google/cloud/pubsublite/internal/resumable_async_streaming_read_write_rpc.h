// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H

#include "google/cloud/pubsublite/internal/futures.h"
#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::internal::BackoffPolicy;
using google::cloud::internal::RetryPolicy;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to create new streams.
 */
template <typename RequestType, typename ResponseType>
using AsyncStreamFactory = std::function<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>()>;

/**
 * `ResumableAsyncStreamingReadWriteRpc<ResponseType, RequestType>` uses
 * callables compatible with this `std::function<>` to initialize a stream
 * from AsyncStreamFactory.
 */
template <typename RequestType, typename ResponseType>
using StreamInitializer = std::function<future<StatusOr<
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>>>(
    std::unique_ptr<AsyncStreamingReadWriteRpc<RequestType, ResponseType>>)>;

using AsyncSleeper = std::function<future<void>(std::chrono::milliseconds)>;

using RetryPolicyFactory = std::function<std::unique_ptr<RetryPolicy>()>;

/**
 * Defines the interface for resumable bidirectional streaming RPCs.
 *
 * Concrete instances of this class resume interrupted streaming RPCs after
 * transient failures. On such failures the concrete implementations would
 * typically create a new streaming RPC and call an asynchronous function to
 * to initialize the stream.
 *
 * While resuming a streaming RPC is automatic, callers of `Read` and `Write`
 * are notified when a new stream is created, as they may need to take action
 * when starting on a new stream.
 *
 * Example (sort of unrealistic) usage:
 * @code
 * using Underlying = std::unique_ptr<StreamingReadWriteRpc<Req, Res>>;
 *
 * // Initializes a non-resumable stream, potentially making many
 * // chained asynchronous calls.
 * future<StatusOr<Underlying>> Initialize(Underlying to_init);
 *
 * Status Example() {
 *   std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<Req, Res>> stream =
 *      MakeResumableStream(&Initialize);
 *   future<Status> final_status = stream->Start(); // 1
 *   while (!final_status.is_ready()) {
 *     if (!stream->Write(GetMessage1()).get()) continue;
 *     if (!stream->Write(GetMessage2()).get()) continue;
 *     auto response_1 = stream->Read().get();
 *     if (!response_1.has_value()) continue;
 *     ProcessResponse(*response_1);
 *     auto response_2 = stream->Read().get();
 *     if (!response_2.has_value()) continue;
 *     ProcessResponse(*response_2);
 *     resumable_stream.Shutdown().get();
 *     return Status();
 *   }
 *   return final_status.get();
 * }
 * @endcode
 */
template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpc {
 public:
  virtual ~ResumableAsyncStreamingReadWriteRpc() = default;

  /**
   * Start the streaming RPC.
   *
   * The future returned by this function is satisfied when the stream
   * is successfully shut down (in which case in contains an ok status),
   * or when the retry policies to resume the stream are exhausted. The
   * latter includes the case where the stream fails with a permanent
   * error.
   *
   * While the stream is usable immediately after this function returns,
   * any `Read()` or `Write()` calls will fail until the stream is initialized
   * successfully.
   */
  virtual future<Status> Start() = 0;

  /**
   * Read one response from the streaming RPC.
   *
   * @note Only **one** `Read()` operation may be pending at a time. The
   * application is responsible for waiting until any previous `Read()`
   * operations have completed before calling `Read()` again.
   *
   * Whether `Read()` can be called before a `Write()` operation is specified by
   * each service and RPC. Most services require at least one `Write()` call
   * before calling `Read()`. Many services may return more than one response
   * for a single `Write()` request.  Each service and RPC specifies how to
   * discover if more responses will be forthcoming.
   *
   * The future returned by `Read` will be satisfied when the `Read` call on the
   * underlying stream successfully completes or when the internal retry loop
   * (un)successfully completes if the underlying call to `Read` fails.
   *
   * If the future is satisfied with an engaged `optional<>`, it holds a value
   * read from the current underlying GRPC stream. If the future is satisfied
   * with `nullopt`, the underlying stream may have changed or a permanent error
   * has happened. If the `Start` future is not satisfied, the user may call
   * `Read` again to read from a new underlying stream.
   */
  virtual future<absl::optional<ResponseType>> Read() = 0;

  /**
   * Write one request to the streaming RPC.
   *
   * @note Only **one** `Write()` operation may be pending at a time. The
   * application is responsible for waiting until any previous `Write()`
   * operations have completed before calling `Write()` again.
   *
   * Whether `Write()` can be called before waiting for a matching `Read()`
   * operation is specified by each service and RPC. Many services tolerate
   * multiple `Write()` calls before performing or at least receiving a `Read()`
   * response.
   *
   * The future returned by `Write` will be satisfied when the `Write` call on
   * the underlying stream successfully completes or when the internal retry
   * loop (un)successfully completes if the underlying call to `Write` fails.
   *
   * If the future is satisfied with `true`, a successful `Write` call was made
   * to the current underlying GRPC stream. If the future is satisfied with
   * `false`, the underlying stream may have changed or a permanent error has
   * happened. If the `Start` future is not satisfied, the user may call `Write`
   * again to write the value to a new underlying stream.
   */
  virtual future<bool> Write(RequestType const&, grpc::WriteOptions) = 0;

  /**
   * Finishes the streaming RPC.
   *
   * This will cause any outstanding `Read` or `Write` to fail. This may be
   * called while a `Read` or `Write` of an object of this class is outstanding.
   * Internally, the class will manage waiting on `Read` and `Write` calls on a
   * gRPC stream before calling `Finish` on its underlying stream as per
   * `google::cloud::AsyncStreamingReadWriteRpc`. If the class is currently in a
   * retry loop, this will terminate the retry loop and then satisfy the
   * returned future. If the class has a present internal outstanding `Read` or
   * `Write`, this call will satisfy the returned future only after the internal
   * `Read` and/or `Write` finish.
   */
  virtual future<void> Shutdown() = 0;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
