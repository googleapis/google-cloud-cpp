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

#include "google/cloud/internal/source_transforms.h"
#include "google/cloud/internal/source_accumulators.h"
#include "google/cloud/internal/source_builder.h"
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

TEST(TransformedSource, Simple) {
  auto transformed =
      MakeTransformedSource(FakeSource<int, Status>({1, 2, 3, 4}, Status{}),
                            [](int x) { return std::to_string(x); });
  auto const actual = MakeSourceBuilder(std::move(transformed))
                          .Accumulate<AccumulateAllEvents>()
                          .get();
  ASSERT_EQ(actual.index(), 0);  // expect success
  EXPECT_THAT(absl::get<0>(actual), ElementsAre("1", "2", "3", "4"));
}

TEST(TransformedSource, Error) {
  auto const expected = Status{StatusCode::kPermissionDenied, "uh-oh"};
  auto transformed =
      MakeTransformedSource(FakeSource<int, Status>({1, 2, 3, 4}, expected),
                            [](int x) { return std::to_string(x); });
  auto const actual = MakeSourceBuilder(std::move(transformed))
                          .Accumulate<AccumulateAllEvents>()
                          .get();
  ASSERT_EQ(actual.index(), 1);  // expect error
  EXPECT_THAT(absl::get<1>(actual), expected);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
