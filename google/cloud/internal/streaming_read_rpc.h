// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_H

#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/sync_stream.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Defines the interface for wrappers around gRPC streaming read RPCs.
 *
 * We wrap the gRPC classes used for streaming read RPCs to (a) simplify the
 * memory management of auxiliary data structures, (b) enforce the "rules"
 * around calling `Finish()` before deleting an RPC, (c) allow us to mock the
 * classes, and (d) allow us to decorate the streaming RPCs, for example for
 * logging.
 *
 * This class defines the interface for these wrappers.  The canonical
 * implementation is `StreamingReadRpcImpl<ResponseType>`
 */
template <typename ResponseType>
class StreamingReadRpc {
 public:
  virtual ~StreamingReadRpc() = default;

  /// Cancel the RPC, this is needed to terminate the RPC "early".
  virtual void Cancel() = 0;

  /// Return the next element, or the final RPC status.
  virtual absl::variant<Status, ResponseType> Read() = 0;
};

/// Report the errors in a standalone function to minimize includes
void StreamingReadRpcReportUnhandledError(Status const& status,
                                          char const* tname);

/**
 * Implement `StreamingReadRpc<ResponseType>` using the gRPC abstractions.
 *
 * @note this class is thread compatible, but it is not thread safe. It should
 *   not be used from multiple threads at the same time.
 */
template <typename ResponseType>
class StreamingReadRpcImpl : public StreamingReadRpc<ResponseType> {
 public:
  StreamingReadRpcImpl(
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<grpc::ClientReaderInterface<ResponseType>> stream)
      : context_(std::move(context)), stream_(std::move(stream)) {}

  ~StreamingReadRpcImpl() override {
    if (finished_) return;
    Cancel();
    auto status = Finish();
    if (status.ok()) return;
    StreamingReadRpcReportUnhandledError(status, typeid(ResponseType).name());
  }

  void Cancel() override { context_->TryCancel(); }

  absl::variant<Status, ResponseType> Read() override {
    ResponseType response;
    if (stream_->Read(&response)) return response;
    return Finish();
  }

 private:
  Status Finish() {
    auto status = MakeStatusFromRpcError(stream_->Finish());
    finished_ = true;
    return status;
  }

  std::unique_ptr<grpc::ClientContext> const context_;
  std::unique_ptr<grpc::ClientReaderInterface<ResponseType>> const stream_;
  bool finished_ = false;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_H
