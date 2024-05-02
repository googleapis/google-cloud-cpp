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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_IMPL_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/grpc_request_metadata.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include "absl/types/optional.h"
#include <grpcpp/support/async_stream.h>
#include <memory>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Wrapper for Asynchronous Streaming Read RPCs.
 *
 * A wrapper for gRPC's asynchronous streaming read APIs, which can be
 * combined with `google::cloud::CompletionQueue` and `google::cloud::future<>`
 * to provide easier-to-use abstractions than the objects returned by gRPC.
 */
template <typename Response>
class AsyncStreamingReadRpcImpl : public AsyncStreamingReadRpc<Response> {
 public:
  AsyncStreamingReadRpcImpl(
      std::shared_ptr<CompletionQueueImpl> cq,
      std::shared_ptr<grpc::ClientContext> context, ImmutableOptions options,
      std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>> stream)
      : cq_(std::move(cq)),
        context_(std::move(context)),
        options_(std::move(options)),
        stream_(std::move(stream)) {}

  void Cancel() override { context_->TryCancel(); }

  future<bool> Start() override {
    struct OnStart : public AsyncGrpcOperation {
      explicit OnStart(ImmutableOptions o) : call_context(std::move(o)) {}

      bool Notify(bool ok) override {
        ScopedCallContext scope(call_context);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}

      promise<bool> p;
      CallContext call_context;
    };
    auto op = std::make_shared<OnStart>(options_);
    cq_->StartOperation(op, [&](void* tag) { stream_->StartCall(tag); });
    return op->p.get_future();
  }

  future<absl::optional<Response>> Read() override {
    struct OnRead : public AsyncGrpcOperation {
      explicit OnRead(ImmutableOptions o) : call_context(std::move(o)) {}

      bool Notify(bool ok) override {
        ScopedCallContext scope(call_context);
        if (!ok) {
          p.set_value({});
          return true;
        }
        p.set_value(std::move(response));
        return true;
      }
      void Cancel() override {}

      promise<absl::optional<Response>> p;
      Response response;
      CallContext call_context;
    };
    auto op = std::make_shared<OnRead>(options_);
    cq_->StartOperation(op,
                        [&](void* tag) { stream_->Read(&op->response, tag); });
    return op->p.get_future();
  }

  future<Status> Finish() override {
    struct OnFinish : public AsyncGrpcOperation {
      explicit OnFinish(ImmutableOptions o) : call_context(std::move(o)) {}

      bool Notify(bool /*ok*/) override {
        ScopedCallContext scope(call_context);
        p.set_value(MakeStatusFromRpcError(std::move(status)));
        return true;
      }
      void Cancel() override {}

      promise<Status> p;
      CallContext call_context;
      grpc::Status status;
    };
    auto op = std::make_shared<OnFinish>(options_);
    cq_->StartOperation(op,
                        [&](void* tag) { stream_->Finish(&op->status, tag); });
    return op->p.get_future();
  }

  RpcMetadata GetRequestMetadata() const override {
    return GetRequestMetadataFromContext(*context_,
                                         /*is_initial_metadata_ready=*/true);
  }

 private:
  std::shared_ptr<CompletionQueueImpl> cq_;
  std::shared_ptr<grpc::ClientContext> context_;
  ImmutableOptions options_;
  std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>> stream_;
};

template <typename Request, typename Response>
using PrepareAsyncReadRpc = absl::FunctionRef<
    std::unique_ptr<grpc::ClientAsyncReaderInterface<Response>>(
        grpc::ClientContext*, Request const&, grpc::CompletionQueue*)>;

/**
 * Make an asynchronous streaming read RPC using `CompletionQueue`.
 *
 * @note In the past we would have made this a member function of the
 *     `CompletionQueue` class. We want to avoid this as (a) we are not certain
 *     this is the long term API we want to expose, (b) once in the public
 *     `CompletionQueue` class it is hard to remove member functions.  Placing
 *     the API in the `internal::` namespace gives us more flexibility for the
 *     future, at the cost of (hopefully controlled) breaks in encapsulation.
 */
template <typename Request, typename Response>
std::unique_ptr<AsyncStreamingReadRpc<Response>> MakeStreamingReadRpc(
    CompletionQueue const& cq, std::shared_ptr<grpc::ClientContext> context,
    ImmutableOptions options, Request const& request,
    PrepareAsyncReadRpc<Request, Response> async_call) {
  auto cq_impl = GetCompletionQueueImpl(cq);
  auto stream = async_call(context.get(), request, cq_impl->cq());
  return std::make_unique<AsyncStreamingReadRpcImpl<Response>>(
      std::move(cq_impl), std::move(context), std::move(options),
      std::move(stream));
}

/**
 * An asynchronous streaming read RPC returning a fixed error.
 *
 * This is used when the library cannot even start the streaming RPC, for
 * example, because setting up the credentials for the call failed.  One could
 * return `StatusOr<std::unique_ptr<AsyncStreamingReadRpc<Response>>` in such
 * cases. We represent the error as part of the stream, as the receiving code
 * must deal with streams that fail anyway.
 */
template <typename Response>
class AsyncStreamingReadRpcError : public AsyncStreamingReadRpc<Response> {
 public:
  explicit AsyncStreamingReadRpcError(Status status)
      : status_(std::move(status)) {}
  ~AsyncStreamingReadRpcError() override = default;

  void Cancel() override {}
  future<bool> Start() override { return make_ready_future(false); }
  future<absl::optional<Response>> Read() override {
    return make_ready_future<absl::optional<Response>>(absl::nullopt);
  }
  future<Status> Finish() override { return make_ready_future(status_); }
  RpcMetadata GetRequestMetadata() const override { return {}; }

 private:
  Status status_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_IMPL_H
