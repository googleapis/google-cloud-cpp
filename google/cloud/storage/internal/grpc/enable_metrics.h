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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_ENABLE_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_ENABLE_METRICS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
#include <opentelemetry/sdk/resource/resource.h>
#include <functional>
#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void EnableGrpcMetrics(Options const& storage_options);

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
/**
 * Overload of `EnableGrpcMetrics` that takes an explicit resource detector for
 * testing.
 *
 * Invokes @p detect_resource only when metrics export is enabled for
 * @p storage_options, avoiding an unnecessary metadata server query otherwise.
 */
void EnableGrpcMetrics(
    Options const& storage_options,
    std::function<opentelemetry::sdk::resource::Resource()> const&
        detect_resource);
#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_ENABLE_METRICS_H
