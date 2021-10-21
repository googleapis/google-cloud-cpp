// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/app_profile_config.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
TEST(AppProfileConfig, MultiClusterUseAny) {
  auto proto = AppProfileConfig::MultiClusterUseAny("my-profile").as_proto();
  EXPECT_EQ("my-profile", proto.app_profile_id());
  EXPECT_TRUE(proto.app_profile().has_multi_cluster_routing_use_any());

  // Verify that as_proto() for rvalue-references returns the right type.
  static_assert(std::is_rvalue_reference<
                    decltype(std::move(std::declval<AppProfileConfig>())
                                 .as_proto())>::value,
                "Return type from as_proto() must be rvalue-reference");
}

TEST(AppProfileConfig, SetIgnoreWarnings) {
  auto proto = AppProfileConfig::MultiClusterUseAny("my-profile")
                   .set_ignore_warnings(true)
                   .as_proto();
  EXPECT_TRUE(proto.ignore_warnings());
}

TEST(AppProfileConfig, SetDescription) {
  auto proto = AppProfileConfig::MultiClusterUseAny("my-profile")
                   .set_description("my description")
                   .as_proto();
  EXPECT_EQ("my description", proto.app_profile().description());
}

TEST(AppProfileConfig, SingleClusterRouting) {
  auto proto =
      AppProfileConfig::SingleClusterRouting("my-profile", "the-cluster", false)
          .as_proto();
  EXPECT_EQ("my-profile", proto.app_profile_id());
  ASSERT_TRUE(proto.app_profile().has_single_cluster_routing());
  auto const& routing = proto.app_profile().single_cluster_routing();
  EXPECT_EQ("the-cluster", routing.cluster_id());
  EXPECT_FALSE(routing.allow_transactional_writes());
}

TEST(AppProfileConfig, SingleClusterRoutingWithTransactionalWrites) {
  auto proto =
      AppProfileConfig::SingleClusterRouting("my-profile", "the-cluster", true)
          .as_proto();
  EXPECT_EQ("my-profile", proto.app_profile_id());
  ASSERT_TRUE(proto.app_profile().has_single_cluster_routing());
  auto const& routing = proto.app_profile().single_cluster_routing();
  EXPECT_EQ("the-cluster", routing.cluster_id());
  EXPECT_TRUE(routing.allow_transactional_writes());
}

bool HasFieldNameOnce(google::protobuf::FieldMask const& mask,
                      std::string const& name) {
  return std::count(mask.paths().begin(), mask.paths().end(), name) == 1;
}

TEST(AppProfileUpdateConfig, SetDescription) {
  auto proto =
      AppProfileUpdateConfig().set_description("a description").as_proto();
  EXPECT_EQ("a description", proto.app_profile().description());
  EXPECT_TRUE(HasFieldNameOnce(proto.update_mask(), "description"));
}

TEST(AppProfileUpdateConfig, SetETag) {
  auto proto = AppProfileUpdateConfig().set_etag("xyzzy").as_proto();
  EXPECT_EQ("xyzzy", proto.app_profile().etag());
  EXPECT_TRUE(HasFieldNameOnce(proto.update_mask(), "etag"));
}

TEST(AppProfileUpdateConfig, SetMultiClusterUseAny) {
  auto proto = AppProfileUpdateConfig().set_multi_cluster_use_any().as_proto();
  EXPECT_TRUE(proto.app_profile().has_multi_cluster_routing_use_any());
  EXPECT_TRUE(
      HasFieldNameOnce(proto.update_mask(), "multi_cluster_routing_use_any"));
}

TEST(AppProfileUpdateConfig, SetSingleClusterRouting) {
  auto proto = AppProfileUpdateConfig()
                   .set_single_cluster_routing("c1", true)
                   .as_proto();
  EXPECT_TRUE(proto.app_profile().has_single_cluster_routing());
  EXPECT_EQ("c1", proto.app_profile().single_cluster_routing().cluster_id());
  EXPECT_TRUE(proto.app_profile()
                  .single_cluster_routing()
                  .allow_transactional_writes());
  EXPECT_TRUE(HasFieldNameOnce(proto.update_mask(), "single_cluster_routing"));
}

TEST(AppProfileUpdateConfig, SetSeveral) {
  auto proto = AppProfileUpdateConfig()
                   .set_description("foo")
                   .set_description("bar")
                   .set_etag("e1")
                   .set_etag("abcdef")
                   .set_multi_cluster_use_any()
                   .set_single_cluster_routing("c1", true)
                   .as_proto();
  EXPECT_EQ("bar", proto.app_profile().description());
  EXPECT_TRUE(HasFieldNameOnce(proto.update_mask(), "description"));
  EXPECT_EQ("abcdef", proto.app_profile().etag());
  EXPECT_TRUE(HasFieldNameOnce(proto.update_mask(), "etag"));
  EXPECT_FALSE(proto.app_profile().has_multi_cluster_routing_use_any());
  EXPECT_TRUE(proto.app_profile().has_single_cluster_routing());
  EXPECT_EQ("c1", proto.app_profile().single_cluster_routing().cluster_id());
  EXPECT_TRUE(proto.app_profile()
                  .single_cluster_routing()
                  .allow_transactional_writes());
  EXPECT_FALSE(HasFieldNameOnce(proto.update_mask(), "multi_cluster_use_any"));
  EXPECT_TRUE(HasFieldNameOnce(proto.update_mask(), "single_cluster_routing"));
}
}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
