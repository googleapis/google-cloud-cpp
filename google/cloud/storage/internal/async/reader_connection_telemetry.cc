// Copyright 2026 Google LLC
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

#include "google/cloud/storage/internal/async/reader_connection_telemetry.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/version.h"
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/trace/context.h>
#endif
#include <chrono>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
namespace {
struct ReadLatencyMetrics {
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      queue_hist;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      network_hist;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      output_hist;

  static ReadLatencyMetrics const& Instance() {
    static ReadLatencyMetrics const metrics = [] {
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter =
          opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
              "google-cloud-cpp", version_string());
      return ReadLatencyMetrics{
          meter->CreateDoubleHistogram("gl-cpp.latency.bidi_read.queue",
                                       "Read Range Queue Latency", "us"),
          meter->CreateDoubleHistogram("gl-cpp.latency.bidi_read.network",
                                       "Read Range Network Latency", "us"),
          meter->CreateDoubleHistogram("gl-cpp.latency.bidi_read.internal",
                                       "Read Range Internal Overhead", "us"),
      };
    }();
    return metrics;
  }
};
}  // namespace

void ReaderConnectionTelemetry::RecordMetrics(
    std::string const& bucket_name, double p1, double p2, double p3,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span)
    const {
  auto const& metrics = ReadLatencyMetrics::Instance();
  auto context = opentelemetry::context::RuntimeContext::GetCurrent();
  if (span && span->GetContext().IsValid()) {
    context = opentelemetry::trace::SetSpan(context, span);
  }
  if (metrics.queue_hist) {
    metrics.queue_hist->Record(p1, {{"gcp.storage.bucket", bucket_name}},
                               context);
  }
  if (metrics.network_hist) {
    metrics.network_hist->Record(p2, {{"gcp.storage.bucket", bucket_name}},
                                 context);
  }
  if (metrics.output_hist) {
    metrics.output_hist->Record(p3, {{"gcp.storage.bucket", bucket_name}},
                                context);
  }
}
#endif

void ReaderConnectionTelemetry::RecordRead(
    storage::ReadPayload const& payload,
    std::chrono::steady_clock::time_point t7, std::string const& bucket_name,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const& span,
    std::string_view event_name) const {
  std::chrono::steady_clock::time_point t4 = ReadPayloadImpl::GetT4(payload);
  std::chrono::steady_clock::time_point t5 = ReadPayloadImpl::GetT5(payload);
  std::chrono::steady_clock::time_point t6 = ReadPayloadImpl::GetT6(payload);

  if (t4 == std::chrono::steady_clock::time_point{} || t4 > t5 || t5 > t6 ||
      t6 > t7) {
    return;
  }

  double p1 = std::chrono::duration<double, std::micro>(t5 - t4).count();
  double p2 = std::chrono::duration<double, std::micro>(t6 - t5).count();
  double p3 = std::chrono::duration<double, std::micro>(t7 - t6).count();

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
  RecordMetrics(bucket_name, p1, p2, p3, span);
#else
  (void)bucket_name;
  (void)p1;
  (void)p2;
  (void)p3;
#endif

  if (span && span->GetContext().IsValid()) {
    span->AddEvent(
        opentelemetry::nostd::string_view{event_name.data(), event_name.size()},
        {{"gl-cpp.latency.queue", p1},
         {"gl-cpp.latency.network", p2},
         {"gl-cpp.latency.internal", p3}});
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
