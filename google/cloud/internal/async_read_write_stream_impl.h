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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_IMPL_H

#include "google/cloud/async_streaming_read_write_rpc.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/grpc_error_delegate.h"
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
 * Wrapper for Asynchronous Streaming Read/Write RPCs.
 *
 * A wrapper for gRPC's asynchronous streaming read-write APIs, which can be
 * combined with `google::cloud::CompletionQueue` and `google::cloud::future<>`
 * to provide easier-to-use abstractions.
 */
template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcImpl
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  AsyncStreamingReadWriteRpcImpl(
      std::shared_ptr<CompletionQueueImpl> cq,
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<grpc::ClientAsyncReaderWriterInterface<Request, Response>>
          stream)
      : cq_(std::move(cq)),
        context_(std::move(context)),
        stream_(std::move(stream)) {}

  void Cancel() override { context_->TryCancel(); }

  future<bool> Start() override {
    struct OnStart : public AsyncGrpcOperation {
      promise<bool> p;
      Options options_ = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options_);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnStart>();
    cq_->StartOperation(op, [&](void* tag) { stream_->StartCall(tag); });
    return op->p.get_future();
  }

  future<absl::optional<Response>> Read() override {
    struct OnRead : public AsyncGrpcOperation {
      promise<absl::optional<Response>> p;
      Response response;
      Options options_ = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options_);
        if (!ok) {
          p.set_value({});
          return true;
        }
        p.set_value(std::move(response));
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnRead>();
    cq_->StartOperation(op,
                        [&](void* tag) { stream_->Read(&op->response, tag); });
    return op->p.get_future();
  }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions options) override {
    struct OnWrite : public AsyncGrpcOperation {
      promise<bool> p;
      Options options_ = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options_);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnWrite>();
    cq_->StartOperation(op, [&](void* tag) {
      stream_->Write(request, std::move(options), tag);
    });
    return op->p.get_future();
  }

  future<bool> WritesDone() override {
    struct OnWritesDone : public AsyncGrpcOperation {
      promise<bool> p;
      Options options_ = CurrentOptions();
      bool Notify(bool ok) override {
        OptionsSpan span(options_);
        p.set_value(ok);
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnWritesDone>();
    cq_->StartOperation(op, [&](void* tag) { stream_->WritesDone(tag); });
    return op->p.get_future();
  }

  future<Status> Finish() override {
    struct OnFinish : public AsyncGrpcOperation {
      promise<Status> p;
      Options options_ = CurrentOptions();
      grpc::Status status;
      bool Notify(bool /*ok*/) override {
        OptionsSpan span(options_);
        p.set_value(MakeStatusFromRpcError(std::move(status)));
        return true;
      }
      void Cancel() override {}
    };
    auto op = std::make_shared<OnFinish>();
    cq_->StartOperation(op,
                        [&](void* tag) { stream_->Finish(&op->status, tag); });
    return op->p.get_future();
  }

 private:
  std::shared_ptr<CompletionQueueImpl> cq_;
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<grpc::ClientAsyncReaderWriterInterface<Request, Response>>
      stream_;
};

template <typename Request, typename Response>
using PrepareAsyncReadWriteRpc = absl::FunctionRef<
    std::unique_ptr<grpc::ClientAsyncReaderWriterInterface<Request, Response>>(
        grpc::ClientContext*, grpc::CompletionQueue*)>;

/**
 * Make an asynchronous streaming read/write RPC using `CompletionQueue`.
 *
 * @note In the past we would have made this a member function of the
 *     `CompletionQueue` class. We want to avoid this as (a) we are not certain
 *     this is the long term API we want to expose, (b) once in the public
 *     `CompletionQueue` class it is hard to remove member functions.  Placing
 *     the API in the `internal::` namespace give us more flexibility for the
 *     future, at the cost of (hopefully controlled) breaks in encapsulation.
 */
template <typename Request, typename Response>
std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>>
MakeStreamingReadWriteRpc(
    CompletionQueue const& cq, std::unique_ptr<grpc::ClientContext> context,
    PrepareAsyncReadWriteRpc<Request, Response> async_call) {
  auto cq_impl = GetCompletionQueueImpl(cq);
  auto stream = async_call(context.get(), cq_impl->cq());
  return absl::make_unique<AsyncStreamingReadWriteRpcImpl<Request, Response>>(
      std::move(cq_impl), std::move(context), std::move(stream));
}

/**
 * A streaming read-write RPC returning a fixed error.
 *
 * This is used when the library cannot even start the streaming RPC, for
 * example, because setting up the credentials for the call failed.  One could
 * return `StatusOr<std::unique_ptr<StreamingWriteRpc<A, B>>` in such cases, but
 * the receiving code must deal with streams that fail anyway. It seems more
 * elegant to represent the error as part of the stream.
 */
template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcError
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  explicit AsyncStreamingReadWriteRpcError(Status status)
      : status_(std::move(status)) {}
  ~AsyncStreamingReadWriteRpcError() override = default;

  void Cancel() override {}
  future<bool> Start() override { return make_ready_future(false); }
  future<absl::optional<Response>> Read() override {
    return make_ready_future<absl::optional<Response>>(absl::nullopt);
  }
  future<bool> Write(Request const&, grpc::WriteOptions) override {
    return make_ready_future(false);
  }
  future<bool> WritesDone() override { return make_ready_future(false); }
  future<Status> Finish() override { return make_ready_future(status_); }

 private:
  Status status_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_IMPL_H
