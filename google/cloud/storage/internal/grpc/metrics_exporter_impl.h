// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_EXPORTER_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_EXPORTER_IMPL_H

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ExporterConfig {
  Project project;
  Options exporter_options;
  Options exporter_connection_options;
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      reader_options;
  std::string authority;
};

absl::optional<ExporterConfig> MakeMeterProviderConfig(
    opentelemetry::sdk::resource::Resource const& resource,
    Options const& options);

void EnableGrpcMetricsImpl(ExporterConfig config);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_EXPORTER_IMPL_H
