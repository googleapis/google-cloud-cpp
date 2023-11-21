// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_TRACING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_TRACING_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/grpc_request_metadata.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Tracing decorator for StreamingWriteRpc.
 */
template <typename RequestType, typename ResponseType>
class StreamingWriteRpcTracing
    : public StreamingWriteRpc<RequestType, ResponseType> {
 public:
  StreamingWriteRpcTracing(
      std::shared_ptr<grpc::ClientContext> context,
      std::unique_ptr<StreamingWriteRpc<RequestType, ResponseType>> impl,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span)
      : context_(std::move(context)),
        impl_(std::move(impl)),
        span_(std::move(span)) {}

  ~StreamingWriteRpcTracing() override {
    EndSpan(*std::move(context_), *std::move(span_), Status());
  }

  void Cancel() override {
    span_->AddEvent("gl-cpp.cancel");
    impl_->Cancel();
  }

  bool Write(RequestType const& request, grpc::WriteOptions options) override {
    auto is_last = options.is_last_message();
    auto success = impl_->Write(request, std::move(options));
    span_->AddEvent("message", {{"message.type", "SENT"},
                                {"message.id", ++write_count_},
                                {"message.is_last", is_last},
                                {"message.success", success}});
    return success;
  }

  StatusOr<ResponseType> Close() override {
    span_->AddEvent("gl-cpp.close");
    return EndSpan(*std::move(context_), *std::move(span_), impl_->Close());
  }

  RpcMetadata GetRequestMetadata() const override {
    return impl_->GetRequestMetadata();
  }

 private:
  std::shared_ptr<grpc::ClientContext> context_;
  std::unique_ptr<StreamingWriteRpc<RequestType, ResponseType>> impl_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  int write_count_ = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_STREAMING_WRITE_RPC_TRACING_H
