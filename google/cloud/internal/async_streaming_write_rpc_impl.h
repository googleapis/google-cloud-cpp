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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_IMPL_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/async_streaming_write_rpc.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include "absl/types/optional.h"
#include <grpcpp/support/async_stream.h>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Wrapper for Asynchronous Streaming Write RPCs.
 *
 * A wrapper for gRPC's asynchronous streaming write APIs, which can be
 * combined with `google::cloud::CompletionQueue` and `google::cloud::future<>`
 * to provide easier-to-use abstractions than the objects returned by gRPC.
 */
template <typename Request, typename Response>
class AsyncStreamingWriteRpcImpl
    : public AsyncStreamingWriteRpc<Request, Response> {
 public:
  AsyncStreamingWriteRpcImpl(
      std::shared_ptr<CompletionQueueImpl> cq,
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<Response> response,
      std::unique_ptr<grpc::ClientAsyncWriterInterface<Request>> stream)
      : cq_(std::move(cq)),
        context_(std::move(context)),
        response_(std::move(response)),
        stream_(std::move(stream)) {}

  void Cancel() override { context_->TryCancel(); }

  future<bool> Start() override {
    struct OnStart : public AsyncGrpcOperation {
      promise<bool> p;
      Options options = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnStart>();
    cq_->StartOperation(op, [&](void* tag) { stream_->StartCall(tag); });
    return op->p.get_future();
  }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions write_options) override {
    struct OnWrite : public AsyncGrpcOperation {
      promise<bool> p;
      Options options = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnWrite>();
    cq_->StartOperation(op, [&](void* tag) {
      stream_->Write(request, std::move(write_options), tag);
    });
    return op->p.get_future();
  }

  future<bool> WritesDone() override {
    struct OnWritesDone : public AsyncGrpcOperation {
      promise<bool> p;
      Options options = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnWritesDone>();
    cq_->StartOperation(op, [&](void* tag) { stream_->WritesDone(tag); });
    return op->p.get_future();
  }

  future<StatusOr<Response>> Finish() override {
    struct OnFinish : public AsyncGrpcOperation {
      std::unique_ptr<Response> response;
      promise<StatusOr<Response>> p;
      Options options = CurrentOptions();
      grpc::Status status;
      bool Notify(bool /*ok*/) override {
        OptionsSpan span(options);
        if (status.ok()) {
          p.set_value(std::move(*response));
          return true;
        }
        p.set_value(MakeStatusFromRpcError(std::move(status)));
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnFinish>();
    op->response = std::move(response_);
    cq_->StartOperation(op,
                        [&](void* tag) { stream_->Finish(&op->status, tag); });
    return op->p.get_future();
  }

  StreamingRpcMetadata GetRequestMetadata() const override {
    return GetRequestMetadataFromContext(*context_);
  }

 private:
  std::shared_ptr<CompletionQueueImpl> cq_;
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<Response> response_;
  std::unique_ptr<grpc::ClientAsyncWriterInterface<Request>> stream_;
};

template <typename Request, typename Response>
using PrepareAsyncWriteRpc = absl::FunctionRef<
    std::unique_ptr<grpc::ClientAsyncWriterInterface<Request>>(
        grpc::ClientContext*, Response*, grpc::CompletionQueue*)>;

/**
 * Make an asynchronous streaming write RPC using `CompletionQueue`.
 *
 * @note In the past we would have made this a member function of the
 *     `CompletionQueue` class. We want to avoid this as (a) we are not certain
 *     this is the long term API we want to expose, (b) once in the public
 *     `CompletionQueue` class it is hard to remove member functions.  Placing
 *     the API in the `internal::` namespace gives us more flexibility for the
 *     future, at the cost of (hopefully controlled) breaks in encapsulation.
 */
template <typename Request, typename Response>
std::unique_ptr<AsyncStreamingWriteRpc<Request, Response>>
MakeStreamingWriteRpc(CompletionQueue const& cq,
                      std::unique_ptr<grpc::ClientContext> context,
                      PrepareAsyncWriteRpc<Request, Response> async_call) {
  auto cq_impl = GetCompletionQueueImpl(cq);
  auto response = absl::make_unique<Response>();
  auto stream = async_call(context.get(), response.get(), cq_impl->cq());
  return absl::make_unique<AsyncStreamingWriteRpcImpl<Request, Response>>(
      std::move(cq_impl), std::move(context), std::move(response),
      std::move(stream));
}

/**
 * An asynchronous streaming write RPC returning a fixed error.
 *
 * This is used when the library cannot even start the streaming RPC, for
 * example, because setting up the credentials for the call failed.  One could
 * return `StatusOr<std::unique_ptr<AsyncStreamingWriteRpc<Request, Response>>`
 * in such cases. We represent the error as part of the stream, as the receiving
 * code must deal with streams that fail anyway.
 */
template <typename Request, typename Response>
class AsyncStreamingWriteRpcError
    : public AsyncStreamingWriteRpc<Request, Response> {
 public:
  explicit AsyncStreamingWriteRpcError(Status status)
      : status_(std::move(status)) {}
  ~AsyncStreamingWriteRpcError() override = default;

  void Cancel() override {}
  future<bool> Start() override { return make_ready_future(false); }
  future<bool> Write(Request const&, grpc::WriteOptions) override {
    return make_ready_future(false);
  }
  future<bool> WritesDone() override { return make_ready_future(false); }
  future<StatusOr<Response>> Finish() override {
    return make_ready_future(StatusOr<Response>(status_));
  }
  StreamingRpcMetadata GetRequestMetadata() const override { return {}; }

 private:
  Status status_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_IMPL_H
