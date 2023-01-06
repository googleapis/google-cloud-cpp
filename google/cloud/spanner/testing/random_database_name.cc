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

#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string RandomDatabasePrefixRegex() {
  return R"re(^db-\d{4,}-\d{2}-\d{2}-)re";
}

std::string RandomDatabasePrefix(std::chrono::system_clock::time_point tp) {
  return "db-" + google::cloud::internal::FormatUtcDate(tp) + "-";
}

std::string RandomDatabaseName(google::cloud::internal::DefaultPRNG& generator,
                               std::chrono::system_clock::time_point tp) {
  // A database ID must be between 2 and 30 characters, fitting the regular
  // expression `[a-z][a-z0-9_-]*[a-z0-9]`. We omit underscores and hyphens
  // from the generated suffix to aid readability.
  std::size_t const max_size = 30;
  auto const prefix = RandomDatabasePrefix(tp);
  auto const suffix_size = static_cast<int>(max_size - prefix.size());
  return prefix +
         google::cloud::internal::Sample(
             generator, suffix_size, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
