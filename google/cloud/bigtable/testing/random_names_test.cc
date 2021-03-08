// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/testing/random_names.h"
#include <gmock/gmock.h>
#include <random>
#include <regex>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
namespace {

using ::google::cloud::internal::DefaultPRNG;

TEST(BigtableRandomNames, RandomTableId) {
  using std::chrono::hours;
  DefaultPRNG generator(std::random_device{}());
  auto const now = std::chrono::system_clock::now();
  auto const current = RandomTableId(generator, now);
  auto const older = RandomTableId(generator, now - hours(48));
  EXPECT_LT(older, current);
  auto constexpr kMaxLength = 50;
  EXPECT_LT(older.size(), kMaxLength);
  EXPECT_LT(current.size(), kMaxLength);

  auto re = std::regex(RandomTableIdRegex());
  EXPECT_TRUE(std::regex_match(current, re));
  EXPECT_TRUE(std::regex_match(older, re));
}

TEST(BigtableRandomNames, RandomBackupId) {
  using std::chrono::hours;
  DefaultPRNG generator(std::random_device{}());
  auto const now = std::chrono::system_clock::now();
  auto const current = RandomBackupId(generator, now);
  auto const older = RandomBackupId(generator, now - hours(48));
  EXPECT_LT(older, current);
  auto constexpr kMaxLength = 50;
  EXPECT_LT(older.size(), kMaxLength);
  EXPECT_LT(current.size(), kMaxLength);

  auto re = std::regex(RandomBackupIdRegex());
  EXPECT_TRUE(std::regex_match(current, re));
  EXPECT_TRUE(std::regex_match(older, re));
}

TEST(BigtableRandomNames, RandomClusterId) {
  using std::chrono::hours;
  DefaultPRNG generator(std::random_device{}());
  auto const now = std::chrono::system_clock::now();
  auto const current = RandomClusterId(generator, now);
  auto const older = RandomClusterId(generator, now - hours(48));
  EXPECT_LT(older, current);
  auto constexpr kMaxLength = 30;
  EXPECT_LT(older.size(), kMaxLength);
  EXPECT_LT(current.size(), kMaxLength);

  auto re = std::regex(RandomClusterIdRegex());
  EXPECT_TRUE(std::regex_match(current, re));
  EXPECT_TRUE(std::regex_match(older, re));
}

TEST(BigtableRandomNames, RandomInstanceId) {
  using std::chrono::hours;
  DefaultPRNG generator(std::random_device{}());
  auto const now = std::chrono::system_clock::now();
  auto const current = RandomInstanceId(generator, now);
  auto const older = RandomInstanceId(generator, now - hours(48));
  EXPECT_LT(older, current);
  auto constexpr kMaxLength = 27;
  EXPECT_LT(older.size(), kMaxLength);
  EXPECT_LT(current.size(), kMaxLength);

  auto re = std::regex(RandomInstanceIdRegex());
  EXPECT_TRUE(std::regex_match(current, re));
  EXPECT_TRUE(std::regex_match(older, re));
}

}  // namespace
}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
