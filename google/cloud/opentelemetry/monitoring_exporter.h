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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_MONITORING_EXPORTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_MONITORING_EXPORTER_H

#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/opentelemetry/internal/recordable.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Change formatting for metric names.
 *
 * The default formatter prefixes the name with "workload.googleapis.com/".
 * Note the trailing slash.
 *
 * @see https://cloud.google.com/monitoring/api/v3/naming-conventions for
 *   understanding Google's naming conventions.
 *
 * Common prefixes for [user metrics] are:
 * - "workload.googleapis.com/"
 * - "custom.googleapis.com/"
 * - "external.googleapis.com/user/"
 *
 * There are many [external metrics]. A common one is [Prometheus]:
 * - "external.googleapis.com/prometheus/"
 *
 * [external metrics]: https://cloud.google.com/monitoring/api/metrics_other
 * [prometheus]: https://prometheus.io/
 * [user metrics]: https://cloud.google.com/monitoring/custom-metrics#identifier
 */
struct MetricNameFormatterOption {
  using Type = std::function<std::string(std::string)>;
};

/**
 * Export Google-defined metrics.
 *
 * Set to true if exporting Google-defined metrics. This option is only relevant
 * to Google applications and libraries. It can be ignored by external
 * developers.
 */
struct ServiceTimeSeriesOption {
  using Type = bool;
};

/**
 * Override the monitored resource to tie metrics to.
 *
 * This option is primarily relevant to Google applications and libraries. It
 * can be ignored by external developers.
 */
struct MonitoredResourceOption {
  using Type = google::api::MonitoredResource;
};

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    Project project,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel

// TODO: move this to a file in google/cloud/opentelemetry/internal
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using MonitoredResourceFromDataFn =
    std::function<google::api::MonitoredResource(
        opentelemetry::sdk::metrics::ResourceMetrics const&)>;

using ResourceFilterDataFn = std::function<bool(std::string const&)>;

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(Project project, MonitoredResourceFromDataFn resource_fn,
                       ResourceFilterDataFn filter_fn, Options options = {});

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    Project project, MonitoredResourceFromDataFn resource_fn,
    ResourceFilterDataFn filter_fn,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_MONITORING_EXPORTER_H
