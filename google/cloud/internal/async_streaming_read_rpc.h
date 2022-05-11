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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_H

#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <grpcpp/support/async_stream.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * An abstraction for asynchronous streaming read RPCs.
 *
 * Streaming read RPCs (sometimes called server-side streaming RPCs) allow
 * applications to receive multiple "responses" on the same request.  They are
 * often used in services where:
 *
 * - The results may be large, and sending them back in a single response would
 *   consume too much memory.
 * - A paginated API would be too slow, as it requires a round trip for each
 *   page of results.
 */
template <typename Response>
class AsyncStreamingReadRpc {
 public:
  virtual ~AsyncStreamingReadRpc() = default;

  /**
   * Sends a best-effort request to cancel the RPC.
   *
   * The library code should still wait for the current operation(s) (any
   * pending `Start()`, or `Read()`) to complete. After they complete, the
   * library code should use `Finish()` to determine the status of the RPC.
   */
  virtual void Cancel() = 0;

  /**
   * Start the streaming RPC.
   *
   * The library code should invoke `Start()`, and wait for its result, before
   * calling `Read()`. If `Start()` completes with `false` the stream has
   * completed with an error.  The library code should not invoke `Read()` in
   * this case. On errors, the library code should call`Finish()` to determine
   * the status of the streaming RPC.
   */
  virtual future<bool> Start() = 0;

  /**
   * Read one response from the streaming RPC.
   *
   * @note Only **one** `Read()` operation may be pending at a time. The
   * library code is responsible for waiting until any previous `Read()`
   * operations have completed before calling `Read()` again.
   *
   * If the `optional<>` is not engaged, the streaming RPC has completed.  The
   * library should then call `Finish()` to find out if the streaming RPC
   * was successful or completed with an error.
   */
  virtual future<absl::optional<Response>> Read() = 0;

  /**
   * Return the final status of the streaming RPC.
   *
   * Streaming RPCs may return an error instead of gracefully closing the
   * stream.
   *
   * The application must wait until all pending `Read()` operations have
   * completed before calling `Finish()`.
   */
  virtual future<Status> Finish() = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_H
