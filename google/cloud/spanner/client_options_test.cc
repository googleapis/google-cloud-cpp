// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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

TEST(ClientOptionsTest, OptionsConversionEmpty) {
  ClientOptions const client_options;
  auto const options = Options(client_options);
  EXPECT_FALSE(options.has<QueryOptimizerVersionOption>());
  EXPECT_FALSE(options.has<QueryOptimizerStatisticsPackageOption>());
  EXPECT_FALSE(options.has<RequestPriorityOption>());
  EXPECT_FALSE(options.has<RequestTagOption>());
}

TEST(ClientOptionsTest, OptionsConversionFull) {
  auto query_options = QueryOptions{}
                           .set_optimizer_version("1")
                           .set_optimizer_statistics_package("latest")
                           .set_request_priority(RequestPriority::kHigh)
                           .set_request_tag("tag");
  ClientOptions client_options;
  client_options.set_query_options(std::move(query_options));
  auto const options = Options(client_options);
  EXPECT_TRUE(options.has<QueryOptimizerVersionOption>());
  EXPECT_EQ(options.get<QueryOptimizerVersionOption>(), "1");
  EXPECT_TRUE(options.has<QueryOptimizerStatisticsPackageOption>());
  EXPECT_EQ(options.get<QueryOptimizerStatisticsPackageOption>(), "latest");
  EXPECT_TRUE(options.has<RequestPriorityOption>());
  EXPECT_EQ(options.get<RequestPriorityOption>(), RequestPriority::kHigh);
  EXPECT_TRUE(options.has<RequestTagOption>());
  EXPECT_EQ(options.get<RequestTagOption>(), "tag");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
