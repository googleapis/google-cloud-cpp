// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_STREAMING_READ_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_STREAMING_READ_WRITE_RPC_H

#include "google/cloud/future.h"
#include "google/cloud/rpc_metadata.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <grpcpp/support/async_stream.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A streaming read-write RPC
 *
 * Streaming read-write RPCs (sometimes called bidirectional streaming RPCs)
 * allow applications to send multiple "requests" and receive multiple
 * "responses" on the same request.  They are often used in services where
 * sending one request at a time introduces too much latency. The requests
 */
template <typename Request, typename Response>
class AsyncStreamingReadWriteRpc {
 public:
  virtual ~AsyncStreamingReadWriteRpc() = default;

  /**
   * Sends a best-effort request to cancel the RPC.
   *
   * The application should still wait for the current operation(s) (any pending
   * `Start()`, `Read()`, or `Write*()` requests) to complete and use `Finish()`
   * to determine the status of the RPC.
   */
  virtual void Cancel() = 0;

  /**
   * Start the streaming RPC.
   *
   * Applications should call `Start()` and wait for its result before calling
   * `Read()` and/or `Write()`. If `Start()` completes with `false` the stream
   * has completed with an error.  The application should not call `Read()` or
   * `Write()` in this case. On errors, the application should call`Finish()` to
   * determine the status of the streaming RPC.
   */
  virtual future<bool> Start() = 0;

  /**
   * Read one response from the streaming RPC.
   *
   * @note Only **one** `Read()` operation may be pending at a time. The
   * application is responsible for waiting until any previous `Read()`
   * operations have completed before calling `Read()` again.
   *
   * Whether `Read()` can be called before a `Write()` operation is specified by
   * each service and RPC. Most services require at least one `Write()` call
   * before calling `Read()`.  Many services may return more than one response
   * for a single `Write()` request.  Each service and RPC specifies how to
   * discover if more responses will be forthcoming.
   *
   * If the `optional<>` is not engaged the streaming RPC has completed.  The
   * application should wait until any other pending operations (typically any
   * other `Write()` calls) complete and then call `Finish()` to find the status
   * of the streaming RPC.
   */
  virtual future<absl::optional<Response>> Read() = 0;

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
   * If `Write()` completes with `false` the streaming RPC has completed.   The
   * application should wait until any other pending operations (typically any
   * other `Read()` calls) complete and then call `Finish()` to find the status
   * of the streaming RPC.
   */
  virtual future<bool> Write(Request const&, grpc::WriteOptions) = 0;

  /**
   * Half-closes the streaming RPC.
   *
   * Sends an indication to the service that no more requests will be issued by
   * the client.
   *
   * If `WritesDone()` completes with `false` the streaming RPC has completed.
   * The application should wait until any other pending operations (typically
   * any other `Read()` calls) complete and then call `Finish()` to find the
   * status of the streaming RPC.
   */
  virtual future<bool> WritesDone() = 0;

  /**
   * Return the final status of the streaming RPC.
   *
   * Streaming RPCs may return an error because the stream is closed,
   * independently of any whether the application has called `WritesDone()` or
   * signaled that the stream is closed using other mechanisms (some RPCs define
   * specific attributes to "close" the stream).
   *
   * The application must wait until all pending `Read()` and `Write()`
   * operations have completed before calling `Finish()`.
   */
  virtual future<Status> Finish() = 0;

  /**
   * Return the request metadata.
   *
   * Request metadata is useful for troubleshooting, but may be relatively
   * expensive to extract.  Application developers should avoid this function in
   * the critical path.
   *
   * @note Only call this function once, and only after `Finish()` completes.
   */
  virtual RpcMetadata GetRequestMetadata() const { return {}; }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_ASYNC_STREAMING_READ_WRITE_RPC_H
