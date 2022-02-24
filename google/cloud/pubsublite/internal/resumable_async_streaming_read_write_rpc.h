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
 * The purpose of this class is to encapsulate the retry logic of a generic
 * bidirectional asynchronous RPC stream.
 *
 * Example (unrealistic) usage:
 * ```
 * std::string ExampleReadUsage() {
 *   ResumableAsyncStreamingReadWriteRpc<Request, Response> resumable_stream;
 *   auto start = resumable_stream.Start();
 *   size_t num_attempts = 0;
 *   while (num_attempts < 3) {
 *     auto read_response = resumable_stream.Read().get();
 *     if (read_response) return read_response->Stringify();
 *     // the second conditional is redundant since the only way `Start` can
 * // finish without a user calling `Finish` is if the internal retry loop
 * // finishes with a permanent error (not ok) status.
 *     if (start.is_ready() && !start.get().ok()) {
 *       return "The read response optional was not engaged, and `Start`
 * finished with a non-ok`Status`, so there had to be a permanent error in the
 * retry loop";
 *     }
 *     // we are ready to call `Read` again due to the object's internal retry
 * mechanism.
 *     ++num_attempts;
 *   }
 *   resumable_stream.Finish().get();
 *   return "We finished the stream";
 * }
 * ```
 */
template <typename RequestType, typename ResponseType>
class ResumableAsyncStreamingReadWriteRpc {
 public:
  virtual ~ResumableAsyncStreamingReadWriteRpc() = default;

  /**
   * Start the streaming RPC.
   *
   * The returned future will be returned with a status
   * when this stream will not be resumed or when the user calls `Finish()`. If
   * the stream fails and the retry loop terminates on permanent error, the
   * `Status` returned will be the `Status` that the stream failed with. In the
   * case that there are no errors from `Start`ing the stream and on the latest
   * `Read` and `Write` calls if present, this will return an ok `Status` when
   * the user calls `Finish`.
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
   * If the `optional<>` is engaged, a successful `ResponseType` is
   * returned. If it is not engaged, the call failed, but the user may call
   * `Read` again unless `Start` had finished with a permanent error `Status`.
   *
   * If the `Read` call fails internally on the underlying member stream object,
   * then this class will continually try to reestablish another connection
   * until successfully reestablishing the connection, permanent error, or the
   * user calls `Finish`. A call to `Read` will finish only when a successful
   * `Read` call is made internally on the member stream or the retry loop
   * finishes if the class is currently retrying the connection.
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
   * If `true` is returned, the call was successful. If `false` is returned,
   * the call failed, but the user may call `Write` again unless `Start` had
   * finished with a permanent error `Status`.
   *
   * If the `Write` call fails internally on the underlying member stream
   * object, then this class will continually try to reestablish another
   * connection until successfully reestablishing the connection, permanent
   * error, or the user calls `Finish`. A call to `Write` will finish only when
   * a successful `Write` call is made internally on the member stream or the
   * retry loop finishes if the class is currently retrying the connection.
   */
  virtual future<bool> Write(RequestType const&, grpc::WriteOptions) = 0;

  /**
   * Finishes the streaming RPC.
   *
   * This will cause any outstanding `Read` or `Write` to fail. This may be
   * called while a `Read` or `Write` of an object of this class is outstanding.
   * Internally, the class will manage waiting on `Read` and `Write` calls on a
   * gRPC stream before calling `Finish` as per
   * `google::cloud::AsyncStreamingReadWriteRpc`. If the class is currently in a
   * retry loop, this will terminate the retry loop and then finish the returned
   * future. If the class has a present internal outstanding `Read` or `Write`,
   * this call will finish the returned future only after the internal `Read`
   * and/or `Write` finishes.
   */
  virtual future<void> Finish() = 0;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_RESUMABLE_ASYNC_STREAMING_READ_WRITE_RPC_H
