// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/internal/format_time_point.h"
#include <chrono>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

std::string RandomId(std::string prefix, std::size_t max_size,
                     internal::DefaultPRNG& generator) {
  auto now = std::chrono::system_clock::now();
  prefix.push_back('-');
  prefix.append(internal::FormatUtcDate(now));
  prefix.push_back('-');
  auto const suffix_size = static_cast<int>(max_size - prefix.size());
  return prefix + internal::Sample(generator, suffix_size,
                                   "abcdefghijlkmnopqrstuvwxyz0123456789");
}

}  // namespace

std::string RandomInstanceName(internal::DefaultPRNG& generator) {
  // An instance ID must be between 2 and 64 characters, matching the
  // regular expression `[a-z][-a-z0-9]*[a-z0-9]`.  We omit hyphens from
  // the generated suffix to aid readability.
  return RandomId("temporary-instance", 64, generator);
}

std::string RandomInstanceConfigName(internal::DefaultPRNG& generator) {
  // An instance-config ID must be between 2 and 64 characters, matching the
  // regular expression `custom-[-a-z0-9]*[a-z0-9]`. The `custom-` prefix
  // is required to avoid name conflicts with Google-managed configurations.
  // We omit hyphens from the generated suffix to aid readability.
  return RandomId("custom-temporary-config", 64, generator);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
