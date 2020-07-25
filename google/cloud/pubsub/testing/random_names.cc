// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/internal/format_time_point.h"
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator,
                          std::string const& prefix) {
  auto constexpr kMaxRandomTopicSuffixLength = 32;
  auto now = std::chrono::system_clock::now();
  std::string date = google::cloud::internal::FormatUtcDate(now);
  auto suffix = google::cloud::internal::Sample(
      generator, kMaxRandomTopicSuffixLength, "abcdefghijklmnopqrstuvwxyz");
  auto p = prefix.empty() ? "cloud-cpp" : prefix;
  return p + "-" + date + "-" + suffix;
}

std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& prefix) {
  auto constexpr kMaxRandomSubscriptionSuffixLength = 32;
  auto suffix = google::cloud::internal::Sample(
      generator, kMaxRandomSubscriptionSuffixLength,
      "abcdefghijklmnopqrstuvwxyz");
  auto p = prefix.empty() ? "cloud-cpp" : prefix;
  return p + "-" + suffix;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google
