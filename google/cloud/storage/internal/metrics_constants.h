// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METRICS_CONSTANTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METRICS_CONSTANTS_H

#include "google/cloud/storage/version.h"
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The instrumentation scope for instruments created by this library.
 *
 * gRPC's own instruments use the `grpc-c++` scope. Using the same scope name
 * as the tracer (`gl-cpp`) keeps the library's telemetry identifiable and
 * lets `MakeGrpcMeterProvider()` attach histogram views to it.
 */
inline constexpr char const* kStorageMeterName = "gl-cpp";

/// Shared OpenTelemetry instrument names referenced by both recorders and
/// exporter/view registration so they cannot drift apart.
inline constexpr std::string_view kOperationsMetric =
    "gcp.storage.client.operations";
inline constexpr std::string_view kRequestDurationMetric =
    "gcp.client.request.duration";
inline constexpr std::string_view kRequestBodySizeMetric =
    "gcp.storage.client.request.body.size";
inline constexpr std::string_view kResponseBodySizeMetric =
    "gcp.storage.client.response.body.size";

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METRICS_CONSTANTS_H
