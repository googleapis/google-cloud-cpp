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

#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(QueryOptionsTest, Values) {
  QueryOptions const default_constructed{};
  EXPECT_FALSE(default_constructed.optimizer_version().has_value());
  EXPECT_FALSE(default_constructed.request_priority().has_value());

  auto copy = default_constructed;
  EXPECT_EQ(copy, default_constructed);

  copy.set_request_priority(RequestPriority::kLow);
  EXPECT_NE(copy, default_constructed);
  copy.set_request_priority(RequestPriority::kHigh);
  EXPECT_NE(copy, default_constructed);

  copy.set_request_priority(absl::optional<RequestPriority>{});
  EXPECT_EQ(copy, default_constructed);

  copy.set_request_tag("foo");
  EXPECT_NE(copy, default_constructed);

  copy.set_request_tag(absl::optional<std::string>{});
  EXPECT_EQ(copy, default_constructed);
}

TEST(QueryOptionsTest, OptimizerVersion) {
  QueryOptions const default_constructed{};
  EXPECT_FALSE(default_constructed.optimizer_statistics_package().has_value());

  auto copy = default_constructed;
  EXPECT_EQ(copy, default_constructed);

  copy.set_optimizer_version("");
  EXPECT_NE(copy, default_constructed);
  EXPECT_EQ("", copy.optimizer_version().value());

  copy.set_optimizer_version("foo");
  EXPECT_NE(copy, default_constructed);
  EXPECT_EQ("foo", copy.optimizer_version().value());

  copy.set_optimizer_version(absl::nullopt);
  EXPECT_EQ(copy, default_constructed);
}

TEST(QueryOptionsTest, OptimizerStatisticsPackage) {
  QueryOptions const default_constructed{};
  EXPECT_FALSE(default_constructed.optimizer_statistics_package().has_value());

  auto copy = default_constructed;
  EXPECT_EQ(copy, default_constructed);

  copy.set_optimizer_statistics_package("");
  EXPECT_NE(copy, default_constructed);
  EXPECT_EQ("", copy.optimizer_statistics_package().value());

  copy.set_optimizer_statistics_package("foo");
  EXPECT_NE(copy, default_constructed);
  EXPECT_EQ("foo", copy.optimizer_statistics_package().value());

  copy.set_optimizer_statistics_package(absl::nullopt);
  EXPECT_EQ(copy, default_constructed);
}

TEST(QueryOptionsTest, FromOptionsEmpty) {
  auto const opts = Options{};
  QueryOptions const query_opts(opts);
  EXPECT_FALSE(query_opts.optimizer_version());
  EXPECT_FALSE(query_opts.optimizer_statistics_package());
  EXPECT_FALSE(query_opts.request_priority());
  EXPECT_FALSE(query_opts.request_tag());
}

TEST(QueryOptionsTest, FromOptionsFull) {
  auto const opts = Options{}
                        .set<QueryOptimizerVersionOption>("1")
                        .set<QueryOptimizerStatisticsPackageOption>("latest")
                        .set<RequestPriorityOption>(RequestPriority::kHigh)
                        .set<RequestTagOption>("tag");
  QueryOptions const query_opts(opts);
  ASSERT_TRUE(query_opts.optimizer_version());
  EXPECT_EQ(*query_opts.optimizer_version(), "1");
  ASSERT_TRUE(query_opts.optimizer_statistics_package());
  EXPECT_EQ(*query_opts.optimizer_statistics_package(), "latest");
  ASSERT_TRUE(query_opts.request_priority());
  EXPECT_EQ(*query_opts.request_priority(), RequestPriority::kHigh);
  ASSERT_TRUE(query_opts.request_tag());
  EXPECT_EQ(*query_opts.request_tag(), "tag");
}

TEST(QueryOptionsTest, OptionsRoundTrip) {
  auto const query_opts = QueryOptions{}
                              .set_optimizer_version("1")
                              .set_optimizer_statistics_package("latest")
                              .set_request_priority(RequestPriority::kHigh)
                              .set_request_tag("tag");
  QueryOptions const rt_query_opts(Options{query_opts});
  ASSERT_TRUE(rt_query_opts.optimizer_version());
  EXPECT_EQ(*rt_query_opts.optimizer_version(), "1");
  ASSERT_TRUE(rt_query_opts.optimizer_statistics_package());
  EXPECT_EQ(*rt_query_opts.optimizer_statistics_package(), "latest");
  ASSERT_TRUE(rt_query_opts.request_priority());
  EXPECT_EQ(*rt_query_opts.request_priority(), RequestPriority::kHigh);
  ASSERT_TRUE(rt_query_opts.request_tag());
  EXPECT_EQ(*rt_query_opts.request_tag(), "tag");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
