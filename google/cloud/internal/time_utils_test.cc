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

#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

TEST(TimeUtils, ConvertTimepointToProtoTimestamp) {
  auto const epoch = std::chrono::system_clock::from_time_t(0);
  auto t = epoch + std::chrono::seconds(123) + std::chrono::nanoseconds(456000);
  auto proto_timestamp = ToProtoTimestamp(t);
  EXPECT_EQ(123, proto_timestamp.seconds());
  EXPECT_EQ(456000, proto_timestamp.nanos());
}

TEST(TimeUtils, ConvertProtoTimestampToChronoTimePoint) {
  google::protobuf::Timestamp proto_timestamp;
  proto_timestamp.set_seconds(867);
  proto_timestamp.set_nanos(530900);
  std::chrono::system_clock::time_point timepoint =
      ToChronoTimePoint(proto_timestamp);

  std::chrono::system_clock::time_point expected =
      std::chrono::system_clock::from_time_t(0) +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::seconds(867) + std::chrono::nanoseconds(530900));
  EXPECT_EQ(timepoint, expected);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
