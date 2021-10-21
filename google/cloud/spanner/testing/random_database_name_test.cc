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

#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::StartsWith;

TEST(RandomDatabaseNameTest, PrefixMatchesRegexp) {
  auto const prefix = RandomDatabasePrefix(std::chrono::system_clock::now());
  auto const re = RandomDatabasePrefixRegex();

  EXPECT_TRUE(std::regex_match(prefix, std::regex(re)));
}

TEST(RandomDatabaseNameTest, NameMatchesRegexp) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const name = RandomDatabaseName(generator);
  auto const re = RandomDatabasePrefixRegex();

  EXPECT_TRUE(std::regex_match(name, std::regex(re)));
}

TEST(RandomDatabaseNameTest, RandomNameHasPrefix) {
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const now = std::chrono::system_clock::now();
  auto const prefix = RandomDatabasePrefix(now);
  auto const name = RandomDatabaseName(generator, now);
  EXPECT_THAT(name, StartsWith(prefix));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
