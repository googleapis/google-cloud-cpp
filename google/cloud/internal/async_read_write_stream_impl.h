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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_IMPL_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/version.h"
#include "absl/functional/function_ref.h"
#include "absl/types/optional.h"
#include <grpcpp/support/async_stream.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <typename Request, typename Response>
class AsyncStreamingReadWriteRpc {
 public:
  virtual ~AsyncStreamingReadWriteRpc() = default;

  virtual void Cancel() = 0;
  virtual future<bool> Start() = 0;
  virtual future<absl::optional<Response>> Read() = 0;
  virtual future<bool> Write(Request const&, grpc::WriteOptions) = 0;
  virtual future<bool> WritesDone() = 0;
  virtual future<Status> Finish() = 0;
};

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
      bool Notify(bool ok) override {
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
      bool Notify(bool ok) override {
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
      bool Notify(bool ok) override {
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
      bool Notify(bool ok) override {
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
      grpc::Status status;
      bool Notify(bool /*ok*/) override {
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
 * @note in the past we would have made this a member function of the
 *     `CompletionQueue` class. We want to avoid this as (a) we are not certain
 *     this is the long term API we want to expose, (b) once in the public
 *     `CompletionQueue` class it is hard to remove member functions.  Placing
 *     the API in the `internal::` namespace give us more flexibility for the
 *     future, at the cost of (hopefully controlled) breaks in encapsulation.
 */
template <typename Request, typename Response>
std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>>
MakeStreamingReadWriteRpc(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    PrepareAsyncReadWriteRpc<Request, Response> async_call) {
  auto cq_impl = GetCompletionQueueImpl(cq);
  auto stream = async_call(context.get(), &cq_impl->cq());
  return absl::make_unique<AsyncStreamingReadWriteRpcImpl<Request, Response>>(
      std::move(cq_impl), std::move(context), std::move(stream));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_IMPL_H
