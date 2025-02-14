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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RESOURCE_DETECTOR_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RESOURCE_DETECTOR_IMPL_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <opentelemetry/sdk/resource/resource_detector.h>
#include <deque>
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using HttpClientFactory =
    std::function<std::unique_ptr<rest_internal::RestClient>(Options const&)>;

/// Defines what error codes are permanent errors.
struct StatusTraits {
  static bool IsPermanentFailure(Status const& status) {
    return status.code() != StatusCode::kInternal &&
           status.code() != StatusCode::kUnavailable;
  }
};
using RetryPolicy = internal::TraitBasedRetryPolicy<StatusTraits>;

std::unique_ptr<opentelemetry::sdk::resource::ResourceDetector>
MakeResourceDetector(HttpClientFactory factory,
                     std::unique_ptr<google::cloud::RetryPolicy> retry,
                     std::unique_ptr<BackoffPolicy> backoff,
                     Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RESOURCE_DETECTOR_IMPL_H
