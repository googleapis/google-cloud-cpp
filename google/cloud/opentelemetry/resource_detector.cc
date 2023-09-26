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

#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/opentelemetry/internal/resource_detector_impl.h"
#include "google/cloud/internal/rest_client.h"
#include <chrono>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeResourceDetector() {
  auto retry = internal::LimitedTimeRetryPolicy<otel_internal::StatusTraits>(
      std::chrono::seconds(10));
  auto backoff =
      ExponentialBackoffPolicy(std::chrono::seconds(0), std::chrono::seconds(1),
                               std::chrono::seconds(5), 2.0, 2.0);
  return otel_internal::MakeResourceDetector(
      [](Options const& options) {
        return rest_internal::MakeDefaultRestClient("", options);
      },
      retry.clone(), backoff.clone(), Options{});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
