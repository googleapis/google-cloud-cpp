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
#include <thread>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

TEST(FormatSize, Basic) {
  EXPECT_EQ("1023.0B", FormatSize(1023));
  EXPECT_EQ("1.0KiB", FormatSize(kKiB));
  EXPECT_EQ("1.1KiB", FormatSize(kKiB + 100));
  EXPECT_EQ("1.0MiB", FormatSize(kMiB));
  EXPECT_EQ("1.0GiB", FormatSize(kGiB));
  EXPECT_EQ("1.1GiB", FormatSize(kGiB + 128 * kMiB));
  EXPECT_EQ("1.0TiB", FormatSize(kTiB));
  EXPECT_EQ("2.0TiB", FormatSize(2 * kTiB));
}

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
