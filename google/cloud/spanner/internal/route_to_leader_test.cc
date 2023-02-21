// Copyright 2023 Google LLC
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

#include "google/cloud/spanner/internal/route_to_leader.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <map>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

class RouteToLeader : public testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  testing_util::ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kRouteToLeader = "x-goog-spanner-route-to-leader";

TEST_F(RouteToLeader, OptionUnset) {
  grpc::ClientContext context;
  {
    // Implicitly allow route-to-leader.
    internal::OptionsSpan span(Options{});
    spanner_internal::RouteToLeader(context);
  }
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Contains(Pair(kRouteToLeader, "true")));
}

TEST_F(RouteToLeader, OptionTrue) {
  grpc::ClientContext context;
  {
    // Explicitly allow route-to-leader.
    internal::OptionsSpan span(
        Options{}.set<spanner::RouteToLeaderOption>(true));
    spanner_internal::RouteToLeader(context);
  }
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Contains(Pair(kRouteToLeader, "true")));
}

TEST_F(RouteToLeader, OptionFalse) {
  grpc::ClientContext context;
  {
    // Explicitly disallow route-to-leader.
    internal::OptionsSpan span(
        Options{}.set<spanner::RouteToLeaderOption>(false));
    spanner_internal::RouteToLeader(context);
  }
  auto metadata = GetMetadata(context);
  EXPECT_THAT(metadata, Not(Contains(Key(kRouteToLeader))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
