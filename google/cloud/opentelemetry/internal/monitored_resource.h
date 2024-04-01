// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_MONITORED_RESOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_MONITORED_RESOURCE_H

#include "google/cloud/version.h"
#include <opentelemetry/sdk/resource/resource.h>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string AsString(
    opentelemetry::sdk::common::OwnedAttributeValue const& attribute);

/*
 * A struct representing a Google Cloud monitored resource.
 *
 * These are resources that are tracked by Cloud Monitoring. See
 * https://cloud.google.com/monitoring/api/resources for a list of such
 * resources.
 */
struct MonitoredResource {
  // e.g. "gce_instance"
  std::string type;
  // e.g. {{"location", "us-central1-a"}}
  std::unordered_map<std::string, std::string> labels;
};

/*
 * Map the attributes to a monitored resource.
 */
MonitoredResource ToMonitoredResource(
    opentelemetry::sdk::resource::ResourceAttributes const& attributes);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_MONITORED_RESOURCE_H
