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

#include "google/cloud/spanner/commit_options.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(CommitOptionsTest, Defaults) {
  CommitOptions options;
  EXPECT_FALSE(options.return_stats());
  EXPECT_FALSE(options.request_priority().has_value());
}

TEST(CommitOptionsTest, SetValues) {
  CommitOptions options;
  options.set_return_stats(true);
  options.set_request_priority(RequestPriority::kLow);
  EXPECT_TRUE(options.return_stats());
  EXPECT_EQ(RequestPriority::kLow, *options.request_priority());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
