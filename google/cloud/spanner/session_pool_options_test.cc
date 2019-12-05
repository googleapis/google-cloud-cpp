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

#include "google/cloud/spanner/session_pool_options.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(SessionPoolOptionsTest, MinSessions) {
  SessionPoolOptions options;
  options.set_min_sessions(-1).EnforceConstraints(/*num_channels=*/1);
  EXPECT_EQ(0, options.min_sessions());
}

TEST(SessionPoolOptionsTest, MaxSessionsPerChannel) {
  SessionPoolOptions options;
  options.set_max_sessions_per_channel(0).EnforceConstraints(
      /*num_channels=*/1);
  EXPECT_EQ(1, options.max_sessions_per_channel());
}

TEST(SessionPoolOptionsTest, MaxIdleSessions) {
  SessionPoolOptions options;
  options.set_max_idle_sessions(-1).EnforceConstraints(
      /*num_channels=*/1);
  EXPECT_EQ(0, options.max_idle_sessions());
}

TEST(SessionPoolOptionsTest, MaxMinSessionsConflict) {
  SessionPoolOptions options;
  options.set_min_sessions(10)
      .set_max_sessions_per_channel(2)
      .EnforceConstraints(/*num_channels=*/3);
  EXPECT_EQ(6, options.min_sessions());
  EXPECT_EQ(2, options.max_sessions_per_channel());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
