// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/embedded_server_test_fixture.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class MetadataUpdatePolicyTest : public testing::EmbeddedServerTestFixture {};

using ::testing::HasSubstr;

TEST_F(MetadataUpdatePolicyTest, TableMetadata) {
  grpc::string expected = "table_name=" + std::string(kTableName);
  auto reader = table_->ReadRows(RowSet("row1"), 1, Filter::PassAllFilter());
  // lets make the RPC call to send metadata
  reader.begin();
  // Get metadata from embedded server
  auto client_metadata = bigtable_service_.client_metadata();
  auto range = client_metadata.equal_range("x-goog-request-params");
  ASSERT_EQ(1, std::distance(range.first, range.second));
  EXPECT_EQ(expected, range.first->second);
}

TEST_F(MetadataUpdatePolicyTest, TableWithNewTargetMetadata) {
  std::string const other_project_id = "other-project";
  std::string const other_instance_id = "other-instance";
  std::string const other_table_id = "other-table";
  auto other_table = table_->WithNewTarget(other_project_id, other_instance_id,
                                           other_table_id);

  grpc::string expected =
      "table_name=" +
      TableName(other_project_id, other_instance_id, other_table_id);
  auto reader =
      other_table.ReadRows(RowSet("row1"), 1, Filter::PassAllFilter());
  // lets make the RPC call to send metadata
  reader.begin();
  // Get metadata from embedded server
  auto client_metadata = bigtable_service_.client_metadata();
  auto range = client_metadata.equal_range("x-goog-request-params");
  ASSERT_EQ(1, std::distance(range.first, range.second));
  EXPECT_EQ(expected, range.first->second);
}

/// @test A cloning test for normal construction of metadata .
TEST_F(MetadataUpdatePolicyTest, SimpleDefault) {
  auto const x_google_request_params = "parent=" + std::string(kInstanceName);
  MetadataUpdatePolicy created(kInstanceName, MetadataParamTypes::PARENT);
  EXPECT_EQ(x_google_request_params, created.value());
  EXPECT_THAT(created.api_client_header(), HasSubstr("gl-cpp/"));
  EXPECT_THAT(created.api_client_header(), HasSubstr("gccl/"));
  EXPECT_THAT(created.api_client_header(),
              AnyOf(HasSubstr("-noex-"), HasSubstr("-ex-")));
}

TEST_F(MetadataUpdatePolicyTest, TableAppProfileMetadata) {
  auto table = Table(data_client_, "profile", kTableId);
  grpc::string expected =
      "table_name=" + std::string(kTableName) + "&app_profile_id=profile";
  auto reader = table.ReadRows(RowSet("row1"), 1, Filter::PassAllFilter());
  // lets make the RPC call to send metadata
  reader.begin();
  // Get metadata from embedded server
  auto client_metadata = bigtable_service_.client_metadata();
  auto range = client_metadata.equal_range("x-goog-request-params");
  ASSERT_EQ(1, std::distance(range.first, range.second));
  EXPECT_EQ(expected, range.first->second);
}

/// @test A test for setting metadata when table is known.
TEST_F(MetadataUpdatePolicyTest, TableWithNewTargetAppProfileMetadata) {
  auto table =
      table_->WithNewTarget(kProjectId, kInstanceId, "profile", kTableId);
  grpc::string expected =
      "table_name=" + std::string(kTableName) + "&app_profile_id=profile";
  auto reader = table.ReadRows(RowSet("row1"), 1, Filter::PassAllFilter());
  // lets make the RPC call to send metadata
  reader.begin();
  // Get metadata from embedded server
  auto client_metadata = bigtable_service_.client_metadata();
  auto range = client_metadata.equal_range("x-goog-request-params");
  ASSERT_EQ(1, std::distance(range.first, range.second));
  EXPECT_EQ(expected, range.first->second);
}

TEST(MakeMetadataUpdatePolicyTest, AppProfileRouting) {
  auto m = bigtable_internal::MakeMetadataUpdatePolicy("table", "");
  EXPECT_EQ(m.value(), "table_name=table");

  m = bigtable_internal::MakeMetadataUpdatePolicy("table", "profile");
  EXPECT_EQ(m.value(), "table_name=table&app_profile_id=profile");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
