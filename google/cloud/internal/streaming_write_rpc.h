// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_H

#include "google/cloud/internal/grpc_request_metadata.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Defines the interface for wrappers around gRPC streaming write RPCs.
 *
 * We wrap the gRPC classes used for streaming write RPCs to (a) simplify the
 * memory management of auxiliary data structures, (b) enforce the "rules"
 * around calling `Finish()` before deleting an RPC, (c) allow us to mock the
 * classes, and (d) allow us to decorate the streaming RPCs, for example for
 * logging.
 *
 * This class defines the interface for these wrappers.  The canonical
 * implementation is `StreamingWriteRpcImpl<RequestType, ResponseType>`
 */
template <typename RequestType, typename ResponseType>
class StreamingWriteRpc {
 public:
  virtual ~StreamingWriteRpc() = default;

  /// Cancel the RPC, this is needed to terminate the RPC "early".
  virtual void Cancel() = 0;

  /**
   * Writes a new request message to the stream.
   *
   * Returns `true` if successful, if this operation fails the application
   * should stop using the stream and call `Close()` to find the specific error.
   */
  virtual bool Write(RequestType const& r, grpc::WriteOptions o) = 0;

  /// Half-close the stream and wait for a response.
  virtual StatusOr<ResponseType> Close() = 0;

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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_H
