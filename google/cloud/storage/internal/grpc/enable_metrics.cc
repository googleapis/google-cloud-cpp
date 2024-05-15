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

#include "google/cloud/storage/internal/grpc/enable_metrics.h"
#if GOOGLE_CLOUD_CPP_STORAGE_AUTO_OTEL_METRICS

#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/internal/grpc/metrics_exporter_impl.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void EnableGrpcMetrics(Options const& storage_options) {
  auto options = DefaultOptionsGrpc(storage_options);
  // Avoid running the resource detector if the metrics are disabled.
  if (!options.get<storage_experimental::EnableGrpcMetricsOption>()) return;
  auto detector = otel::MakeResourceDetector();
  auto resources = detector->Detect();
  auto config = MakeMeterProviderConfig(resources, options);
  if (!config) return;
  EnableGrpcMetricsImpl(std::move(*config));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#else

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void EnableGrpcMetrics(Options const&) {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_AUTO_OTEL_METRICS
