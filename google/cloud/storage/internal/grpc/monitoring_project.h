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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_MONITORING_PROJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_MONITORING_PROJECT_H

#ifdef GOOGLE_CLOUD_CPP_STORAGE_AUTO_OTEL_METRICS

#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <opentelemetry/sdk/resource/resource.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns the project to automatically publish GCS+gRPC metrics.
 */
absl::optional<Project> MonitoringProject(
    opentelemetry::sdk::resource::Resource const& resource,
    Options const& options);

/**
 * Returns the project associated with @p credentials, if any.
 */
absl::optional<Project> MonitoringProject(Credentials const& credentials);

/**
 * Returns the project associated with @p resource.
 *
 * This function should be called with the outcome from a `Detect()` call on a
 * GCP resource detector. If the detector finds a GCP project, we can use it as
 * the project to send monitoring metrics.
 */
absl::optional<Project> MonitoringProject(
    opentelemetry::sdk::resource::Resource const& resource);

/**
 * Returns the monitoring project given the (fully populated) options.
 */
absl::optional<Project> MonitoringProject(Options const& options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_AUTO_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_MONITORING_PROJECT_H
