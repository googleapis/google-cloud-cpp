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

#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/embedded_server_test_fixture.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <thread>

namespace bigtable = google::cloud::bigtable;

namespace {
class MetadataUpdatePolicyTest
    : public bigtable::testing::EmbeddedServerTestFixture {};
}  // anonymous namespace

using ::testing::HasSubstr;

/// @test A test for setting metadata for admin operations.
TEST_F(MetadataUpdatePolicyTest, RunWithEmbeddedServer) {
  grpc::string expected = "parent=" + kInstanceName;
  auto gc = bigtable::GcRule::MaxNumVersions(42);
  admin_->CreateTable(kTableName, bigtable::TableConfig({{"fam", gc}}, {}));
  // Get metadata from embedded server
  auto client_metadata = admin_service_.client_metadata();
  auto range = client_metadata.equal_range("x-goog-request-params");
  ASSERT_EQ(1, std::distance(range.first, range.second));
  EXPECT_EQ(expected, range.first->second);
}

/// @test A test for setting metadata when table is not known.
TEST_F(MetadataUpdatePolicyTest, RunWithEmbeddedServerLazyMetadata) {
  grpc::string expected = "name=" + kTableName;
  admin_->GetTable(kTableId);
  // Get metadata from embedded server
  auto client_metadata = admin_service_.client_metadata();
  auto range = client_metadata.equal_range("x-goog-request-params");
  ASSERT_EQ(1, std::distance(range.first, range.second));
  EXPECT_EQ(expected, range.first->second);
}

/// @test A test for setting metadata when table is known.
TEST_F(MetadataUpdatePolicyTest, RunWithEmbeddedServerParamTableName) {
  grpc::string expected = "table_name=" + kTableName;
  auto reader = table_->ReadRows(bigtable::RowSet("row1"), 1,
                                 bigtable::Filter::PassAllFilter());
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
  auto const x_google_request_params = "parent=" + kInstanceName;
  bigtable::MetadataUpdatePolicy created(kInstanceName,
                                         bigtable::MetadataParamTypes::PARENT);
  EXPECT_EQ(x_google_request_params, created.value());
  EXPECT_THAT(created.api_client_header(), HasSubstr("gl-cpp/"));
  EXPECT_THAT(created.api_client_header(), HasSubstr("gccl/"));
  EXPECT_THAT(created.api_client_header(),
              ::testing::AnyOf(HasSubstr("-noex-"), HasSubstr("-ex-")));
}
