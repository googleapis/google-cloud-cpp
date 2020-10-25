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
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {
TEST(StorageBenchmarksUtilsTest, ParseSize) {
  EXPECT_EQ(500, ParseSize("500"));

  EXPECT_EQ(1 * kKB, ParseSize("1KB"));
  EXPECT_EQ(2 * kMB, ParseSize("2MB"));
  EXPECT_EQ(3 * kGB, ParseSize("3GB"));
  EXPECT_EQ(4 * kTB, ParseSize("4TB"));

  EXPECT_EQ(5 * kTiB, ParseSize("5TiB"));
  EXPECT_EQ(6 * kGiB, ParseSize("6GiB"));
  EXPECT_EQ(7 * kMiB, ParseSize("7MiB"));
  EXPECT_EQ(8 * kKiB, ParseSize("8KiB"));
}

TEST(StorageBenchmarksUtilsTest, ParseBufferSize) {
  EXPECT_EQ(500, ParseBufferSize("500"));

  EXPECT_EQ(1 * kKB, ParseBufferSize("1KB"));
  EXPECT_EQ(2 * kMB, ParseBufferSize("2MB"));
  EXPECT_EQ(3 * kGB, ParseBufferSize("3GB"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_ANY_THROW(ParseBufferSize("-2"));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(StorageBenchmarksUtilsTest, ParseDuration) {
  using s = std::chrono::seconds;
  using m = std::chrono::minutes;
  using h = std::chrono::hours;
  EXPECT_EQ(s(m(42)).count(), ParseDuration("42m").count());
  EXPECT_EQ(s(h(3)).count(), ParseDuration("3h").count());
  EXPECT_EQ(s(1800).count(), ParseDuration("1800s").count());
}

TEST(StorageBenchmarksUtilsTest, ParseBoolean) {
  EXPECT_EQ(true, ParseBoolean("").value_or(true));
  EXPECT_EQ(false, ParseBoolean("").value_or(false));

  EXPECT_EQ(true, ParseBoolean("true").value_or(false));
  EXPECT_EQ(true, ParseBoolean("True").value_or(false));

  EXPECT_EQ(false, ParseBoolean("false").value_or(true));
  EXPECT_EQ(false, ParseBoolean("False").value_or(true));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
