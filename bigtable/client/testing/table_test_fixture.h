// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_TABLE_TEST_FIXTURE_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_TABLE_TEST_FIXTURE_H_

#include "bigtable/client/data.h"
#include "bigtable/client/testing/mock_client.h"

namespace bigtable {
namespace testing {

/// Common fixture for the bigtable::Table tests.
class TableTestFixture : public ::testing::Test {
 protected:
  TableTestFixture() {}

  void SetUp() override;

  std::shared_ptr<MockClient> SetupMockClient() {
    auto client = std::make_shared<MockClient>();
    EXPECT_CALL(*client, ProjectId())
        .WillRepeatedly(::testing::ReturnRef(project_id_));
    EXPECT_CALL(*client, InstanceId())
        .WillRepeatedly(::testing::ReturnRef(instance_id_));
    EXPECT_CALL(*client, Stub())
        .WillRepeatedly(::testing::ReturnRef(*bigtable_stub_));
    return client;
  }

  std::string project_id_ = "the-project";
  std::string instance_id_ = "the-instance";
  std::shared_ptr<::google::bigtable::v2::MockBigtableStub> bigtable_stub_ =
      std::make_shared<::google::bigtable::v2::MockBigtableStub>();
  std::shared_ptr<MockClient> client_ = SetupMockClient();
  bigtable::Table table_ = bigtable::Table(client_, "foo-table");
};

}  // namespace testing
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TESTING_TABLE_TEST_FIXTURE_H_
