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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_TEST_FIXTURE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_TEST_FIXTURE_H

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_data_client.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/// Common fixture for the bigtable::Table tests.
class TableTestFixture : public ::testing::Test {
 protected:
  TableTestFixture() {}

  std::shared_ptr<MockDataClient> SetupMockClient();

  std::string const kProjectId = "foo-project";
  std::string const kInstanceId = "bar-instance";
  std::string const kTableId = "baz-table";

  // These are hardcoded, and not computed, because we want to test the
  // computation.
  std::string const kInstanceName =
      "projects/foo-project/instances/bar-instance";
  std::string const kTableName =
      "projects/foo-project/instances/bar-instance/tables/baz-table";

  std::string project_id_ = kProjectId;
  std::string instance_id_ = kInstanceId;
  std::shared_ptr<MockDataClient> client_ = SetupMockClient();
  bigtable::Table table_ = bigtable::Table(client_, kTableId);
};

google::bigtable::v2::ReadRowsResponse ReadRowsResponseFromString(
    std::string const& repr);

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_TEST_FIXTURE_H
