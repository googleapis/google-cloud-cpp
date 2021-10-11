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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_AUTH_H

#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/version.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcAuth
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  using StreamFactory = std::function<
      std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>>(
          std::unique_ptr<grpc::ClientContext>)>;

  AsyncStreamingReadWriteRpcAuth(
      std::unique_ptr<grpc::ClientContext> context,
      std::shared_ptr<GrpcAuthenticationStrategy> auth, StreamFactory factory)
      : context_(std::move(context)),
        auth_(std::move(auth)),
        factory_(std::move(factory)) {}

  void Cancel() override {
    if (context_) return context_->TryCancel();
    if (stream_) return stream_->Cancel();
  }

  future<bool> Start() override {
    using Result = StatusOr<std::unique_ptr<grpc::ClientContext>>;

    return auth_->AsyncConfigureContext(std::move(context_))
        .then([this](future<Result> f) mutable {
          auto context = f.get();
          if (!context) {
            stream_ = absl::make_unique<
                AsyncStreamingReadWriteRpcError<Request, Response>>(
                std::move(context).status());
            return make_ready_future(false);
          }
          stream_ = factory_(*std::move(context));
          return stream_->Start();
        });
  }

  future<absl::optional<Response>> Read() override {
    if (!stream_) return make_ready_future(absl::optional<Response>{});
    return stream_->Read();
  }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions options) override {
    if (!stream_) return make_ready_future(false);
    return stream_->Write(request, std::move(options));
  }

  future<bool> WritesDone() override {
    if (!stream_) return make_ready_future(false);
    return stream_->WritesDone();
  }

  future<Status> Finish() override {
    if (!stream_) {
      return make_ready_future(
          Status(StatusCode::kInvalidArgument,
                 "uninitialized GrpcReadWriteStreamAuth<>"));
    }
    return stream_->Finish();
  }

 private:
  std::unique_ptr<grpc::ClientContext> context_;
  std::shared_ptr<GrpcAuthenticationStrategy> auth_;
  StreamFactory factory_;
  std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> stream_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_AUTH_H
