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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_TRACING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_TRACING_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/async_read_write_stream_impl.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename Request, typename Response>
class AsyncStreamingReadWriteRpcTracing
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  AsyncStreamingReadWriteRpcTracing(
      std::shared_ptr<grpc::ClientContext> context,
      std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> impl,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span)
      : context_(std::move(context)),
        impl_(std::move(impl)),
        span_(std::move(span)) {}
  ~AsyncStreamingReadWriteRpcTracing() override { (void)End(Status()); }

  void Cancel() override {
    span_->AddEvent("cancel");
    impl_->Cancel();
  }

  future<bool> Start() override {
    return impl_->Start().then([this](future<bool> f) {
      auto started = f.get();
      span_->SetAttribute("gcloud.stream_started", started);
      return started;
    });
  }

  future<absl::optional<Response>> Read() override {
    return impl_->Read().then([this](future<absl::optional<Response>> f) {
      auto r = f.get();
      if (r.has_value()) {
        span_->AddEvent("message", {{"message.type", "RECEIVED"},
                                    {"message.id", ++read_count_}});
      }
      return r;
    });
  }

  future<bool> Write(Request const& request,
                     grpc::WriteOptions options) override {
    auto is_last = options.is_last_message();
    return impl_->Write(request, std::move(options))
        .then([this, is_last](future<bool> f) {
          auto success = f.get();
          span_->AddEvent("message", {{"message.type", "SENT"},
                                      {"message.id", ++write_count_},
                                      {"message.is_last", is_last},
                                      {"message.success", success}});
          return success;
        });
  }

  future<bool> WritesDone() override {
    return impl_->WritesDone().then([this](future<bool> f) {
      span_->AddEvent("gcloud.writes_done");
      return f.get();
    });
  }

  future<Status> Finish() override {
    return impl_->Finish().then(
        [this](future<Status> f) { return End(f.get()); });
  }

 private:
  Status End(Status status) {
    if (!context_) return status;
    return EndSpan(*std::move(context_), *std::move(span_), std::move(status));
  }

  std::shared_ptr<grpc::ClientContext> context_;
  std::unique_ptr<AsyncStreamingReadWriteRpc<Request, Response>> impl_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  int read_count_ = 0;
  int write_count_ = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_READ_WRITE_STREAM_TRACING_H
