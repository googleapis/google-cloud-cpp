// Copyright 2022 Google LLC
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

#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

struct Result {
  explicit Result(StreamRange<int> sr) {
    for (auto& sor : sr) {
      if (!sor) {
        final_status = std::move(sor).status();
      } else {
        values.emplace_back(*sor);
      }
    }
  }

  std::vector<int> values;
  Status final_status;
};

TEST(MakeStreamRangeTest, Empty) {
  auto reader = MakeStreamRange<int>({});
  Result result(std::move(reader));
  EXPECT_THAT(result.values, IsEmpty());
  EXPECT_STATUS_OK(result.final_status);
}

TEST(MakeStreamRangeTest, ValuesOnly) {
  auto reader = MakeStreamRange<int>({1, 2, 3});
  Result result(std::move(reader));
  EXPECT_THAT(result.values, ElementsAre(1, 2, 3));
  EXPECT_STATUS_OK(result.final_status);
}

TEST(MakeStreamRangeTest, StatusOnly) {
  auto reader = MakeStreamRange<int>({}, Status(StatusCode::kAborted, "fail"));
  Result result(std::move(reader));
  EXPECT_THAT(result.values, IsEmpty());
  EXPECT_THAT(result.final_status, StatusIs(StatusCode::kAborted));
}

TEST(MakeStreamRangeTest, ValuesThenStatus) {
  auto reader =
      MakeStreamRange<int>({1, 2, 3}, Status(StatusCode::kAborted, "fail"));
  Result result(std::move(reader));
  EXPECT_THAT(result.values, ElementsAre(1, 2, 3));
  EXPECT_THAT(result.final_status, StatusIs(StatusCode::kAborted));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace mocks
}  // namespace cloud
}  // namespace google
