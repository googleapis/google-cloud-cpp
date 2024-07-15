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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_TRACING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_TRACING_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/async_streaming_read_rpc.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename Response>
class AsyncStreamingReadRpcTracing : public AsyncStreamingReadRpc<Response> {
 public:
  AsyncStreamingReadRpcTracing(
      std::shared_ptr<grpc::ClientContext> context,
      std::unique_ptr<AsyncStreamingReadRpc<Response>> impl,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span)
      : context_(std::move(context)),
        impl_(std::move(impl)),
        span_(std::move(span)) {}
  ~AsyncStreamingReadRpcTracing() override { (void)End(Status()); }

  void Cancel() override {
    span_->AddEvent("gl-cpp.cancel");
    impl_->Cancel();
  }

  future<bool> Start() override {
    // It is sufficient to set `span_` as the parent of `start_span`, because
    // the lower levels do not create any spans.
    opentelemetry::trace::StartSpanOptions options;
    options.parent = span_->GetContext();
    auto start_span = internal::MakeSpan("Start", options);
    return impl_->Start().then(
        [this, ss = std::move(start_span)](future<bool> f) {
          EndSpan(*ss);
          auto started = f.get();
          span_->SetAttribute("gl-cpp.stream_started", started);
          started_ = started;
          return started;
        });
  }

  future<absl::optional<Response>> Read() override {
    if (read_count_ == 0) span_->AddEvent("gl-cpp.first-read");
    return impl_->Read().then([this](future<absl::optional<Response>> f) {
      auto r = f.get();
      if (r.has_value()) {
        span_->AddEvent("message", {{"message.type", "RECEIVED"},
                                    {"message.id", ++read_count_}});
      }
      return r;
    });
  }

  future<Status> Finish() override {
    // It is sufficient to set `span_` as the parent of `finish_span`, because
    // the lower levels do not create any spans.
    opentelemetry::trace::StartSpanOptions options;
    options.parent = span_->GetContext();
    auto finish_span = internal::MakeSpan("Finish", options);
    return impl_->Finish().then(
        [this, fs = std::move(finish_span)](future<Status> f) {
          EndSpan(*fs);
          return End(f.get());
        });
  }

  RpcMetadata GetRequestMetadata() const override {
    return impl_->GetRequestMetadata();
  }

 private:
  Status End(Status status) {
    if (!context_) return status;
    if (started_) {
      return EndSpan(*std::move(context_), *std::move(span_), std::move(status));
    } else {
      return EndSpan(*std::move(span_), std::move(status));
    }
  }

  std::shared_ptr<grpc::ClientContext> context_;
  std::unique_ptr<AsyncStreamingReadRpc<Response>> impl_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  int read_count_ = 0;
  bool started_ = false;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_STREAMING_READ_RPC_TRACING_H
