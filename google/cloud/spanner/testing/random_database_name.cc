// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/format_time_point.h"
#include <ctime>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

std::string RandomDatabasePrefixRegex() {
  return R"re(^db-\d{4}-\d{2}-\d{2}-.*$)re";
}

std::string RandomDatabasePrefix(std::chrono::system_clock::time_point tp) {
  auto tm = google::cloud::internal::AsUtcTm(tp);
  std::string date = "1970-01-01";
  std::strftime(&date[0], date.size() + 1, "%Y-%m-%d", &tm);
  return "db-" + date + "-";
}

std::string RandomDatabaseName(google::cloud::internal::DefaultPRNG& generator,
                               std::chrono::system_clock::time_point tp) {
  // A database ID must be between 2 and 30 characters, fitting the regular
  // expression `[a-z][a-z0-9_\-]*[a-z0-9]`
  std::size_t const max_size = 30;
  auto const prefix = RandomDatabasePrefix(tp);
  auto size = static_cast<int>(max_size - 1 - prefix.size());
  return prefix +
         google::cloud::internal::Sample(
             generator, size, "abcdefghijlkmnopqrstuvwxyz012345689_-") +
         google::cloud::internal::Sample(
             generator, 1, "abcdefghijlkmnopqrstuvwxyz0123456789");
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
