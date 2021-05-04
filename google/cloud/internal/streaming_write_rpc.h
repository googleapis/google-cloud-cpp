// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_H

#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
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
  /**
   * Possible results for a Write() operation.
   *
   * A Write() operation can have three possible outcomes:
   * - The Write was successful, continue using the stream, this is represented
   *   by the `absl::monostate` branch
   * - The streaming RPC is now closed, either with an error (the `Status`
   *   branch), or with a successful final response (the `ResponseType`) branch.
   *   maybe as a result of the Write() as result.
   *
   * @note the stream may be closed because of the `Write()` call (e.g., if it
   * contains the `last_message()` bit), or because the service has closed the
   * stream, or because of network or service errors.
   */
  using WriteOutcome = absl::variant<absl::monostate, Status, ResponseType>;

  virtual ~StreamingWriteRpc() = default;

  /// Cancel the RPC, this is needed to terminate the RPC "early".
  virtual void Cancel() = 0;

  /// Return the next element, or the final RPC status.
  virtual WriteOutcome Write(RequestType const& r, grpc::WriteOptions o) = 0;

  /// Half-close the stream and wait for a response.
  virtual StatusOr<ResponseType> WritesDone() = 0;
};

/// Report the errors in a standalone function to minimize includes
void StreamingWriteRpcReportUnhandledError(Status const& status,
                                           char const* tname);

/**
 * Implement `StreamingWriteRpc<RequestType, ResponseType>` using the gRPC
 * abstractions.
 *
 * @note this class is thread compatible, but it is not thread safe. It should
 *   not be used from multiple threads at the same time.
 */
template <typename RequestType, typename ResponseType>
class StreamingWriteRpcImpl
    : public StreamingWriteRpc<RequestType, ResponseType> {
 public:
  using WriteOutcome =
      typename StreamingWriteRpc<RequestType, ResponseType>::WriteOutcome;

  StreamingWriteRpcImpl(
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<ResponseType> response,
      std::unique_ptr<grpc::ClientWriterInterface<RequestType>> stream)
      : context_(std::move(context)),
        response_(std::move(response)),
        stream_(std::move(stream)) {}

  ~StreamingWriteRpcImpl() override {
    if (finished_) return;
    Cancel();
    auto status = Finish();
    if (status.ok()) return;
    StreamingWriteRpcReportUnhandledError(status, typeid(ResponseType).name());
  }

  void Cancel() override { context_->TryCancel(); }

  WriteOutcome Write(RequestType const& r, grpc::WriteOptions o) override {
    auto const result = stream_->Write(r, o);
    if (result) return absl::monostate{};
    auto status = Finish();
    if (!status.ok()) return status;
    return std::move(*response_);
  }

  StatusOr<ResponseType> WritesDone() override {
    (void)stream_->WritesDone();
    auto status = Finish();
    if (!status.ok()) return status;
    return std::move(*response_);
  }

 private:
  Status Finish() {
    auto status = MakeStatusFromRpcError(stream_->Finish());
    finished_ = true;
    return status;
  }

  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<ResponseType> response_;
  std::unique_ptr<grpc::ClientWriterInterface<RequestType>> stream_;
  bool finished_ = false;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_READ_RPC_H
