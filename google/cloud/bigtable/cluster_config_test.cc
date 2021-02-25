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

#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;

TEST(ClusterConfigTest, Constructor) {
  ClusterConfig config("somewhere", 7, ClusterConfig::SSD);
  auto const& proto = config.as_proto();
  EXPECT_EQ("somewhere", proto.location());
  EXPECT_EQ(7, proto.serve_nodes());
  EXPECT_EQ(ClusterConfig::SSD, proto.default_storage_type());
}

TEST(ClusterConfigTest, Move) {
  ClusterConfig config("somewhere", 7, ClusterConfig::HDD);
  auto proto = std::move(config).as_proto();
  // Verify that as_proto() for rvalue-references returns the right type.
  static_assert(
      std::is_rvalue_reference<decltype(
          std::move(std::declval<ClusterConfig>()).as_proto())>::value,
      "Return type from as_proto() must be rvalue-reference");
  EXPECT_EQ("somewhere", proto.location());
  EXPECT_EQ(7, proto.serve_nodes());
  EXPECT_EQ(ClusterConfig::HDD, proto.default_storage_type());
}

TEST(ClusterConfigTest, SetEncryptionConfig) {
  google::bigtable::admin::v2::Cluster::EncryptionConfig encryption;
  encryption.set_kms_key_name("test-only-invalid-kms-key-name");
  auto const actual = ClusterConfig("somewhere", 7, ClusterConfig::HDD)
                          .SetEncryptionConfig(std::move(encryption))
                          .as_proto();
  auto constexpr kText = R"pb(
    location: "somewhere"
    serve_nodes: 7
    default_storage_type: HDD
    encryption_config { kms_key_name: "test-only-invalid-kms-key-name" }
  )pb";
  google::bigtable::admin::v2::Cluster expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
