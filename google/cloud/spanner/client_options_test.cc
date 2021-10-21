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

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(ClientOptionsTest, OptimizerVersion) {
  ClientOptions const default_constructed{};
  EXPECT_EQ(QueryOptions{}, default_constructed.query_options());

  auto copy = default_constructed;
  EXPECT_EQ(copy, default_constructed);

  copy.set_query_options(QueryOptions{}.set_optimizer_version("foo"));
  EXPECT_NE(copy, default_constructed);

  copy.set_query_options(QueryOptions{});
  EXPECT_EQ(copy, default_constructed);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
