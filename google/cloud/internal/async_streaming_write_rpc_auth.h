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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_AUTH_H

#include "google/cloud/internal/async_streaming_write_rpc.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/// A decorator to support Unified Credentials
template <typename Request, typename Response>
class AsyncStreamingWriteRpcAuth
    : public AsyncStreamingWriteRpc<Request, Response> {
 public:
  using StreamFactory =
      std::function<std::unique_ptr<AsyncStreamingWriteRpc<Request, Response>>(
          std::unique_ptr<grpc::ClientContext>)>;

  AsyncStreamingWriteRpcAuth(std::unique_ptr<grpc::ClientContext> context,
                             std::shared_ptr<GrpcAuthenticationStrategy> auth,
                             StreamFactory factory)
      : auth_(std::move(auth)),
        state_(std::make_shared<SharedState>(std::move(factory),
                                             std::move(context))) {}

  void Cancel() override { state_->Cancel(); }

  future<bool> Start() override {
    using Result = StatusOr<std::unique_ptr<grpc::ClientContext>>;
    auto weak = std::weak_ptr<SharedState>(state_);
    return auth_->AsyncConfigureContext(state_->ReleaseInitialContext())
        .then([weak](future<Result> f) mutable {
          if (auto state = weak.lock()) return state->OnStart(f.get());
          return make_ready_future(false);
        });
  }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions write_options) override {
    std::lock_guard<std::mutex> g{state_->mu};
    return state_->stream->Write(request, std::move(write_options));
  }

  future<bool> WritesDone() override {
    std::lock_guard<std::mutex> g{state_->mu};
    return state_->stream->WritesDone();
  }

  future<StatusOr<Response>> Finish() override { return state_->Finish(); }

  StreamingRpcMetadata GetRequestMetadata() const override {
    return state_->GetRequestMetadata();
  }

 private:
  struct SharedState {
    SharedState(StreamFactory factory,
                std::unique_ptr<grpc::ClientContext> initial_context)
        : factory(std::move(factory)),
          initial_context(std::move(initial_context)),
          stream(
              absl::make_unique<AsyncStreamingWriteRpcError<Request, Response>>(
                  Status(StatusCode::kInternal,
                         "Stream is not yet started."))) {}

    std::unique_ptr<grpc::ClientContext> ReleaseInitialContext() {
      std::lock_guard<std::mutex> g{mu};
      return std::move(initial_context);
    }

    future<bool> OnStart(
        StatusOr<std::unique_ptr<grpc::ClientContext>> context) {
      std::lock_guard<std::mutex> g{mu};
      if (cancelled) return make_ready_future(false);
      if (context) {
        stream = factory(*std::move(context));
      } else {
        stream =
            absl::make_unique<AsyncStreamingWriteRpcError<Request, Response>>(
                std::move(context).status());
      }
      return stream->Start();
    }

    future<StatusOr<Response>> Finish() {
      std::lock_guard<std::mutex> g{mu};
      cancelled = true;  // ensure stream is not recreated after Finish
      return stream->Finish();
    }

    StreamingRpcMetadata GetRequestMetadata() {
      std::lock_guard<std::mutex> g{mu};
      return stream->GetRequestMetadata();
    }

    void Cancel() {
      std::lock_guard<std::mutex> g{mu};
      if (cancelled) return;
      cancelled = true;
      if (initial_context) initial_context->TryCancel();
      stream->Cancel();
    }

    StreamFactory const factory;
    std::mutex mu;
    std::unique_ptr<grpc::ClientContext> initial_context;
    std::unique_ptr<AsyncStreamingWriteRpc<Request, Response>> stream;
    bool cancelled = false;
  };

  std::shared_ptr<GrpcAuthenticationStrategy> const auth_;
  std::shared_ptr<SharedState> const state_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_WRITE_RPC_AUTH_H
