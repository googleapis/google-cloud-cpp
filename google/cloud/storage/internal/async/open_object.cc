// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/open_object.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/str_cat.h"
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
#include "google/cloud/internal/opentelemetry.h"
#include <opentelemetry/metrics/provider.h>
#endif
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string RequestParams(
    google::storage::v2::BidiReadObjectRequest const& request) {
  auto const& read_spec = request.read_object_spec();
  if (read_spec.has_routing_token()) {
    return absl::StrCat("bucket=", read_spec.bucket(),
                        "&routing_token=", read_spec.routing_token());
  }
  return absl::StrCat("bucket=", read_spec.bucket());
}

OpenObject::OpenObject(storage_internal::StorageStub& stub, CompletionQueue& cq,
                       std::shared_ptr<grpc::ClientContext> context,
                       google::cloud::internal::ImmutableOptions options,
                       google::storage::v2::BidiReadObjectRequest request)
    : rpc_(std::make_shared<OpenStream>(CreateRpc(
          stub, cq, std::move(context), std::move(options), request))),
      initial_request_(std::move(request)) {}

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
struct StreamOpenMetrics {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      stream_open_latency;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      network_handshake;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      server_metadata_latency;

  static StreamOpenMetrics const& Instance() {
    static auto const metrics = [] {
      auto meter =
          opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
              "storage", "v1");
      return StreamOpenMetrics{
          meter->CreateDoubleHistogram("gl-cpp.latency.stream_open",
                                       "End-to-End Stream Open", "us"),
          meter->CreateDoubleHistogram("gl-cpp.latency.network_handshake",
                                       "Network Handshake", "us"),
          meter->CreateDoubleHistogram("gl-cpp.latency.server_metadata",
                                       "Server Metadata Latency", "us"),
      };
    }();
    return metrics;
  }
};
#endif

future<StatusOr<OpenStreamResult>> OpenObject::Call() {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  t0_ = std::chrono::steady_clock::now();
  span_ = opentelemetry::trace::Tracer::GetCurrentSpan();
#endif
  auto future = promise_.get_future();
  rpc_->Start().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnStart(f.get());
  });
  return future;
}

std::weak_ptr<OpenObject> OpenObject::WeakFromThis() {
  return shared_from_this();
}

std::unique_ptr<OpenStream::StreamingRpc> OpenObject::CreateRpc(
    storage_internal::StorageStub& stub, CompletionQueue& cq,
    std::shared_ptr<grpc::ClientContext> context,
    google::cloud::internal::ImmutableOptions options,
    google::storage::v2::BidiReadObjectRequest const& request) {
  auto p = RequestParams(request);
  if (!p.empty()) context->AddMetadata("x-goog-request-params", std::move(p));
  return stub.AsyncBidiReadObject(cq, std::move(context), std::move(options));
}

void OpenObject::OnStart(bool ok) {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  t1_ = std::chrono::steady_clock::now();
#endif
  if (!ok) return DoFinish();
  rpc_->Write(initial_request_).then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnWrite(f.get());
  });
}

void OpenObject::OnWrite(bool ok) {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  t2_ = std::chrono::steady_clock::now();
#endif
  if (!ok) return DoFinish();
  rpc_->Read().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnRead(f.get());
  });
}

void OpenObject::OnRead(
    std::optional<google::storage::v2::BidiReadObjectResponse> response) {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  auto t3 = std::chrono::steady_clock::now();
  auto const& metrics = StreamOpenMetrics::Instance();

  auto p1 = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(t1_ - t0_).count());
  auto p2 = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2_).count());
  auto p3 = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(t3 - t0_).count());

  auto bucket = initial_request_.read_object_spec().bucket();
  metrics.network_handshake->Record(p1, {{"gcp.storage.bucket", bucket}},
                                    opentelemetry::context::Context{});
  metrics.server_metadata_latency->Record(p2, {{"gcp.storage.bucket", bucket}},
                                          opentelemetry::context::Context{});
  metrics.stream_open_latency->Record(p3, {{"gcp.storage.bucket", bucket}},
                                      opentelemetry::context::Context{});

  if (span_ && span_->GetContext().IsValid()) {
    span_->AddEvent("gl-cpp.open.read",
                    {{"gl-cpp.latency.network_handshake", p1},
                     {"gl-cpp.latency.server_metadata", p2},
                     {"gl-cpp.latency.stream_open", p3}});
  }
#endif
  if (!response) return DoFinish();
  promise_.set_value(OpenStreamResult{std::move(rpc_), std::move(*response)});
}

void OpenObject::DoFinish() {
  rpc_->Finish().then([w = WeakFromThis()](auto f) {
    if (auto self = w.lock()) self->OnFinish(f.get());
  });
}

void OpenObject::OnFinish(Status status) {
  if (!status.ok()) return promise_.set_value(std::move(status));
  // This should not happen, it indicates an EOF on the stream, but we
  // did not ask to close it.
  promise_.set_value(google::cloud::internal::InternalError(
      "could not open stream, but the stream closed successfully",
      GCP_ERROR_INFO()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
