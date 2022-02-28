// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/read_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(ReadOptionsTest, Equality) {
  ReadOptions test_options_0{};
  ReadOptions test_options_1{};
  EXPECT_EQ(test_options_0, test_options_1);

  test_options_0.index_name = "secondary";
  EXPECT_NE(test_options_0, test_options_1);
  test_options_1.index_name = "secondary";
  EXPECT_EQ(test_options_0, test_options_1);

  test_options_0.limit = 42;
  EXPECT_NE(test_options_0, test_options_1);
  test_options_1.limit = 42;
  EXPECT_EQ(test_options_0, test_options_1);

  test_options_0.request_priority = RequestPriority::kLow;
  EXPECT_NE(test_options_0, test_options_1);
  test_options_1.request_priority = RequestPriority::kLow;
  EXPECT_EQ(test_options_0, test_options_1);

  test_options_0.request_tag = "tag";
  EXPECT_NE(test_options_0, test_options_1);
  test_options_1.request_tag = "tag";
  EXPECT_EQ(test_options_0, test_options_1);

  ReadOptions test_options_2 = test_options_0;
  EXPECT_EQ(test_options_0, test_options_2);
}

TEST(ReadOptionsTest, OptionsRoundTrip) {
  for (auto const& index_name : {"", "index"}) {
    ReadOptions ro;
    ro.index_name = index_name;
    for (std::int64_t limit : {0, 42}) {
      ro.limit = limit;
      for (auto const& request_priority :
           {absl::optional<RequestPriority>(absl::nullopt),
            absl::optional<RequestPriority>(RequestPriority::kLow)}) {
        ro.request_priority = request_priority;
        for (auto const& request_tag :
             {absl::optional<std::string>(absl::nullopt),
              absl::optional<std::string>("tag")}) {
          ro.request_tag = request_tag;
          EXPECT_EQ(ro, ToReadOptions(ToOptions(ro)));
        }
      }
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
