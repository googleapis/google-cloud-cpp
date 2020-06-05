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

#include "google/cloud/internal/source_accumulators.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/fake_source.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::FakeSource;
using ::testing::ElementsAre;

TEST(SourceAccumulators, AccumulateAllInt) {
  FakeSource<int, Status> mock({1, 2, 3, 4}, Status{});
  auto const actual = AccumulateAllEvents(std::move(mock)).get();
  ASSERT_EQ(actual.index(), 0);  // expect success
  EXPECT_THAT(absl::get<0>(actual), ElementsAre(1, 2, 3, 4));
}

TEST(SourceAccumulators, AccumulateAllString) {
  FakeSource<std::string, Status> mock({"a", "b", "c", "d"}, Status{});
  auto const actual = AccumulateAllEvents(std::move(mock)).get();
  ASSERT_EQ(actual.index(), 0);  // expect success
  EXPECT_THAT(absl::get<0>(actual), ElementsAre("a", "b", "c", "d"));
}

TEST(SourceAccumulators, AccumulateAllEmpty) {
  FakeSource<int, Status> mock({}, Status{});
  auto const actual = AccumulateAllEvents(std::move(mock)).get();
  ASSERT_EQ(actual.index(), 0);  // expect success
  EXPECT_TRUE(absl::get<0>(actual).empty());
}

TEST(SourceAccumulators, AccumulateAllError) {
  FakeSource<int, Status> mock({},
                               Status{StatusCode::kUnavailable, "try-again"});
  auto const actual = AccumulateAllEvents(std::move(mock)).get();
  ASSERT_EQ(actual.index(), 1);  // expect error
  EXPECT_EQ(absl::get<1>(actual).code(), StatusCode::kUnavailable);
  EXPECT_EQ(absl::get<1>(actual).message(), "try-again");
}

TEST(SourceAccumulators, AccumulateAllErrorAfterData) {
  FakeSource<int, Status> mock({1, 2, 3},
                               Status{StatusCode::kPermissionDenied, "uh-oh"});
  auto const actual = AccumulateAllEvents(std::move(mock)).get();
  ASSERT_EQ(actual.index(), 1);  // expect error
  EXPECT_EQ(absl::get<1>(actual).code(), StatusCode::kPermissionDenied);
  EXPECT_EQ(absl::get<1>(actual).message(), "uh-oh");
}

TEST(SourceAccumulators, AccumulateAllRef) {
  FakeSource<int, Status> mock({1, 2, 3, 4}, Status{});
  auto& ref = mock;
  auto const actual = AccumulateAllEvents(std::move(ref)).get();
  ASSERT_EQ(actual.index(), 0);  // expect success
  EXPECT_THAT(absl::get<0>(actual), ElementsAre(1, 2, 3, 4));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
