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

#include "google/cloud/storage/internal/async/open_object_metrics.h"

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
#include <opentelemetry/metrics/provider.h>
#endif

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
namespace {
struct StreamOpenMetrics {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      network_handshake;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      server_metadata_latency;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      stream_open_latency;

  static StreamOpenMetrics const& Instance() {
    static auto const metrics = [] {
      auto meter =
          opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
              "storage", "v1");
      return StreamOpenMetrics{
          meter->CreateDoubleHistogram("gl-cpp.latency.network_handshake",
                                       "Network Handshake", "us"),
          meter->CreateDoubleHistogram("gl-cpp.latency.server_metadata",
                                       "Server Metadata Latency", "us"),
          meter->CreateDoubleHistogram("gl-cpp.latency.stream_open",
                                       "End-to-End Stream Open", "us")};
    }();
    return metrics;
  }
};
}  // namespace
#endif

void OpenObjectMetrics::RecordCall() {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  t0_ = std::chrono::steady_clock::now();
#endif
}

void OpenObjectMetrics::RecordStart() {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  t1_ = std::chrono::steady_clock::now();
#endif
}

void OpenObjectMetrics::RecordWrite() {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  t2_ = std::chrono::steady_clock::now();
#endif
}

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
void OpenObjectMetrics::RecordMetrics(
    std::string const& bucket, std::chrono::steady_clock::time_point t3) {
  auto const& metrics = StreamOpenMetrics::Instance();
  auto p1 = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(t1_ - t0_).count());
  auto p2 = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2_).count());
  auto p3 = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(t3 - t0_).count());

  metrics.network_handshake->Record(p1, {{"gcp.storage.bucket", bucket}},
                                    opentelemetry::context::Context{});
  metrics.server_metadata_latency->Record(p2, {{"gcp.storage.bucket", bucket}},
                                          opentelemetry::context::Context{});
  metrics.stream_open_latency->Record(p3, {{"gcp.storage.bucket", bucket}},
                                      opentelemetry::context::Context{});
}
#endif

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
void OpenObjectMetrics::RecordRead(
    std::string const& bucket,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span) {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  auto t3 = std::chrono::steady_clock::now();
  RecordMetrics(bucket, t3);
  if (span && span->GetContext().IsValid()) {
    auto p1 = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(t1_ - t0_)
            .count());
    auto p2 = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2_)
            .count());
    auto p3 = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(t3 - t0_)
            .count());
    span->AddEvent("gl-cpp.stream_open.latency",
                   {{"gl-cpp.latency.network_handshake", p1},
                    {"gl-cpp.latency.server_metadata", p2},
                    {"gl-cpp.latency.stream_open", p3}});
  }
#else
  (void)bucket;
  (void)span;
#endif
}
#else
void OpenObjectMetrics::RecordRead(std::string const& bucket) {
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  RecordMetrics(bucket, std::chrono::steady_clock::now());
#else
  (void)bucket;
#endif
}
#endif

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
