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

#include "google/cloud/spanner/internal/time_utils.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

TEST(TimeUtils, ConvertTimePointToProtoTimestamp) {
  auto const epoch = std::chrono::system_clock::from_time_t(0);
  auto t = epoch + std::chrono::seconds(123) + std::chrono::nanoseconds(456000);
  auto proto_timestamp = ConvertTimePointToProtoTimestamp(t);
  EXPECT_STATUS_OK(proto_timestamp);
  EXPECT_EQ(123, proto_timestamp->seconds());
  EXPECT_EQ(456000, proto_timestamp->nanos());
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
