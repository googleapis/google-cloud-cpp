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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_H

#include "google/cloud/future.h"
#include "google/cloud/internal/grpc_request_metadata.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * An abstraction for asynchronous streaming write RPCs.
 *
 * Streaming write RPCs (sometimes called client-side streaming RPCs) allow
 * callers to send multiple "requests" on the same RPC.  They are often
 * used in services where:
 *
 * - The data sent to the service is large, such as uploads or bulk inserts.
 * - Sending the data in small RPCs would be too slow, as each would require
 *   a full round trip to the service.
 *
 * @note Objects of this type should not be destroyed until the future returned
 *     by `Finish()` is satisfied.
 */
template <typename Request, typename Response>
class AsyncStreamingWriteRpc {
 public:
  virtual ~AsyncStreamingWriteRpc() = default;

  /**
   * Sends a best-effort request to cancel the RPC.
   *
   * The library code should still wait for the current operation (any
   * pending `Start()`, or `Write()`) to complete. After they complete, the
   * library code should use `Finish()` to determine the status of the RPC.
   */
  virtual void Cancel() = 0;

  /**
   * Start the streaming RPC.
   *
   * The library code should invoke `Start()`, and wait for its result, before
   * calling `Write()`. If `Start()` completes with `false` the stream has
   * completed with an error.  The library code should not invoke `Write()` in
   * this case. On errors, the library code must call `Finish()` to determine
   * the status of the streaming RPC.
   */
  virtual future<bool> Start() = 0;

  /**
   * Write one request to the streaming RPC.
   *
   * @note Once `Start()` completes, and before any calls to `WritesDone()` this
   *     function can be called multiple times. However, only **one** `Write()`
   *     operation may be pending at a time. The caller is responsible for
   *     waiting until any previous `Write()` operations have completed before
   *     calling `Write()` again.
   *
   * If `Write()` completes with `false` the streaming RPC has completed. The
   * caller must then invoke `Finish()` to find the status of the streaming RPC.
   */
  virtual future<bool> Write(Request const& request,
                             grpc::WriteOptions write_options) = 0;

  /**
   * Half-closes the streaming RPC.
   *
   * Sends an indication to the service that no more requests will be issued by
   * the client.
   *
   * @note The caller should not invoke `WritesDone()` while a `Write()`
   *     operation is pending.  The caller should invoke `WritesDone()` at most
   *     once. The caller can avoid invoking `WritesDone()` by setting the
   *     `.set_last_message()` flag in the last `grpc::WriteOptions` parameter.
   *
   * If `WritesDone()` completes with `true` then the message half-closing the
   * streaming RPC was successfully sent. This is not a confirmation that it was
   * received correctly.  If `WritesDone()` computes with `false` the streaming
   * RPC has some kind of error.
   *
   * Regardless of the completion value for `WritesDone()`, the caller must
   * invoke `Finish()` to find the status of the streaming RPC.
   */
  virtual future<bool> WritesDone() = 0;

  /**
   * Return the final response and status of the streaming RPC.
   *
   * If the streaming RPC completes successfully, the future is satisfied with
   * the value of the response.  Otherwise, the future is satisfied with the
   * error details.
   *
   * The caller must wait until any pending `Write()` or `WritesDone()`
   * operations have completed before calling `Finish()`.
   *
   * @note The caller must guarantee that the streaming RPC lifetime
   *     extends at least until the `Finish()` operation **completes**. That
   *     is, until the returned future is satisfied.
   */
  virtual future<StatusOr<Response>> Finish() = 0;

  /**
   * Return the request metadata.
   *
   * Request metadata is useful for troubleshooting, but may be relatively
   * expensive to extract.  Library developers should avoid this function in
   * the critical path.
   *
   * @note Only call this function once, and only after `Finish()` completes.
   */
  virtual StreamingRpcMetadata GetRequestMetadata() const = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_H
