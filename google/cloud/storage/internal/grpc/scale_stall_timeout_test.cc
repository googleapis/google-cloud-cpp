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
  EXPECT_EQ(ScaleStallTimeout(0s, 1'000, 1'000), 0us);
  EXPECT_EQ(ScaleStallTimeout(0s, 1'000'000, 1'000), 0us);
  EXPECT_EQ(ScaleStallTimeout(0s, 10'000'000, 1'000), 0us);
}

TEST(ScaleStallTimeout, Simple) {
  EXPECT_EQ(ScaleStallTimeout(1s, 100'000'000, 1'000'000), 10'000us);
  EXPECT_EQ(ScaleStallTimeout(3s, 100'000'000, 1'000'000), 30'000us);
  EXPECT_EQ(ScaleStallTimeout(5s, 100'000'000, 1'000'000), 50'000us);
  EXPECT_EQ(ScaleStallTimeout(10s, 100'000'000, 1'000'000), 100'000us);

  EXPECT_EQ(ScaleStallTimeout(1s, 10'000'000, 1'000'000), 100'000us);
  EXPECT_EQ(ScaleStallTimeout(1s, 1'000'000, 1'000'000), 1'000'000us);
  EXPECT_EQ(ScaleStallTimeout(1s, 1'000, 1'000'000), 1'000'000us);
  EXPECT_EQ(ScaleStallTimeout(1s, 1, 1'000'000), 1'000'000us);
}

TEST(ScaleStallTimeout, Unexpected) {
  EXPECT_EQ(ScaleStallTimeout(1s, 0, 1'000'000), 1'000'000us);
  EXPECT_EQ(ScaleStallTimeout(1s, 1'000'000, 0), 0us);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
