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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_TIME_SERIES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_TIME_SERIES_H

#include "google/cloud/version.h"
#include <google/api/metric.pb.h>
#include <google/monitoring/v3/metric_service.pb.h>
#include <opentelemetry/sdk/metrics/metric_reader.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::api::Metric ToMetric(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::PointAttributes const& attributes);

google::monitoring::v3::TimeSeries ToTimeSeries(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::SumPointData const& sum_data);

google::monitoring::v3::TimeSeries ToTimeSeries(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::LastValuePointData const& gauge_data);

google::monitoring::v3::TimeSeries ToTimeSeries(
    opentelemetry::sdk::metrics::MetricData const& metric_data,
    opentelemetry::sdk::metrics::HistogramPointData const& histogram_data);

/**
 * We need to convert from the C++ OpenTelemetry metrics implementation to
 * Cloud Monitoring protos.
 *
 * See go/otel-gcp-metric-exporter-spec for a somewhat outdated specification.
 * Note that this document describes how to convert from [OTLP] -> protos.
 *
 * To add to the fun, in C++, the OpenTelemetry metrics implementation is not
 * based on OTLP. We can look at the [C++ OTLP exporter implementation] to
 * convert from the OpenTelemetry metrics implementation -> OTLP.
 *
 * There is also the golang implementation:
 * https://github.com/GoogleCloudPlatform/opentelemetry-operations-go/blob/babed4870546b78cee69606726961cfd20cbea42/exporter/metric/metric.go#L514
 *
 * [C++ OTLP exporter implementation]:
 * https://github.com/open-telemetry/opentelemetry-cpp/blob/fabd8cc2bc318cb47d5db7322ea9c8cd3f4b847a/exporters/otlp/src/otlp_metric_utils.cc
 * [OTLP]: https://opentelemetry.io/docs/specs/otel/protocol/
 */
google::monitoring::v3::CreateTimeSeriesRequest ToRequest(
    opentelemetry::sdk::metrics::ResourceMetrics const& data);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_TIME_SERIES_H
