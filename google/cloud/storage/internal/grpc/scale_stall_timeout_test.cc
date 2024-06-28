// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/grpc/scale_stall_timeout.h"
#include "google/cloud/testing_util/chrono_output.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using namespace std::chrono_literals;  // NOLINT

TEST(ScaleStallTimeout, WithDisabledTimeout) {
  EXPECT_EQ(ScaleStallTimeout(0s, 1'000, 1'000), 0ms);
  EXPECT_EQ(ScaleStallTimeout(0s, 1'000'000, 1'000), 0ms);
  EXPECT_EQ(ScaleStallTimeout(0s, 10'000'000, 1'000), 0ms);
}

TEST(ScaleStallTimeout, Simple) {
  EXPECT_EQ(ScaleStallTimeout(1s, 100'000'000, 1'000'000), 10ms);
  EXPECT_EQ(ScaleStallTimeout(3s, 100'000'000, 1'000'000), 30ms);
  EXPECT_EQ(ScaleStallTimeout(5s, 100'000'000, 1'000'000), 50ms);
  EXPECT_EQ(ScaleStallTimeout(10s, 100'000'000, 1'000'000), 100ms);

  EXPECT_EQ(ScaleStallTimeout(1s, 10'000'000, 1'000'000), 100ms);
  EXPECT_EQ(ScaleStallTimeout(1s, 1'000'000, 1'000'000), 1'000ms);
  EXPECT_EQ(ScaleStallTimeout(1s, 1'000, 1'000'000), 1'000ms);
  EXPECT_EQ(ScaleStallTimeout(1s, 1, 1'000'000), 1'000ms);

  auto constexpr kMiB = 1024 * 1024;
  EXPECT_EQ(ScaleStallTimeout(10s, 20 * kMiB, 2 * kMiB), 1000ms);
  EXPECT_EQ(ScaleStallTimeout(10s, 10 * kMiB, 2 * kMiB), 2000ms);
}

TEST(ScaleStallTimeout, Unexpected) {
  EXPECT_EQ(ScaleStallTimeout(1s, 0, 1'000'000), 1'000ms);
  EXPECT_EQ(ScaleStallTimeout(1s, 1'000'000, 0), 0ms);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
