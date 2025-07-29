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

#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/monitoring/v3/metric_connection.h"
#include "google/cloud/opentelemetry/internal/monitoring_exporter.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <memory>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
MakeMonitoringExporter(
    Project project,
    std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
    Options options) {
  options = otel_internal::DefaultOptions(std::move(options));
  return std::make_unique<otel_internal::MonitoringExporter>(
      std::move(project), std::move(conn), options);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
