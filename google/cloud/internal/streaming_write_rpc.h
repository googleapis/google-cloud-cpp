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

  bool Write(RequestType const& r, grpc::WriteOptions o) override {
    if (o.is_last_message()) has_last_message_ = true;
    return stream_->Write(r, std::move(o));
  }

  StatusOr<ResponseType> Close() override {
    if (!has_last_message_) (void)stream_->WritesDone();
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
  bool has_last_message_ = false;
};

/**
 * A stream returning a fixed error.
 *
 * This is used when the library cannot even start the streaming RPC, for
 * example, because setting up the credentials for the call failed.  One could
 * return `StatusOr<std::unique_ptr<StreamingWriteRpc<A, B>>` in such cases, but
 * the receiving code must deal with streams that fail anyway. It seems more
 * elegant to represent the error as part of the stream.
 */
template <typename RequestType, typename ResponseType>
class StreamingWriteRpcError
    : public StreamingWriteRpc<RequestType, ResponseType> {
 public:
  explicit StreamingWriteRpcError(Status status) : status_(std::move(status)) {}
  ~StreamingWriteRpcError() override = default;

  void Cancel() override {}

  bool Write(RequestType const&, grpc::WriteOptions) override { return false; }

  StatusOr<ResponseType> Close() override { return status_; }

 private:
  Status status_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_H
