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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {
TEST(StorageBenchmarksUtilsTest, MakeRandomData) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  EXPECT_EQ(16 * kKiB, MakeRandomData(generator, 16 * kKiB).size());
  EXPECT_EQ(2 * kMiB, MakeRandomData(generator, 2 * kMiB).size());

  auto d1 = MakeRandomData(generator, 16 * kKiB);
  auto d2 = MakeRandomData(generator, 16 * kKiB);
  EXPECT_NE(d1, d2);
}

TEST(StorageBenchmarksUtilsTest, MakeRandomObject) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto d1 = MakeRandomObjectName(generator);
  auto d2 = MakeRandomObjectName(generator);
  EXPECT_NE(d1, d2);
}

TEST(StorageBenchmarksUtilsTest, MakeRandomBucket) {
  google::cloud::internal::DefaultPRNG generator =
      google::cloud::internal::MakeDefaultPRNG();

  auto d1 = MakeRandomBucketName(generator, "prefix-");
  auto d2 = MakeRandomBucketName(generator, "prefix-");
  EXPECT_NE(d1, d2);

  EXPECT_EQ(0, d1.rfind("prefix-", 0));
  EXPECT_GE(63U, d1.size());
  EXPECT_EQ(std::string::npos,
            d1.find_first_not_of("-abcdefghijklmnopqrstuvwxyz012456789"));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
