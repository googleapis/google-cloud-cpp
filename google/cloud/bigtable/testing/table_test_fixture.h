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
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/// Common fixture for the bigtable::Table tests.
class TableTestFixture : public ::testing::Test {
 protected:
  explicit TableTestFixture(CompletionQueue cq);
  std::shared_ptr<MockDataClient> SetupMockClient();

  static char const kProjectId[];
  static char const kInstanceId[];
  static char const kTableId[];
  static char const kInstanceName[];
  static char const kTableName[];

  std::string project_id_ = kProjectId;
  std::string instance_id_ = kInstanceId;
  std::shared_ptr<::google::cloud::testing_util::FakeCompletionQueueImpl>
      cq_impl_;
  CompletionQueue cq_;
  std::shared_ptr<MockDataClient> client_;
  bigtable::Table table_ = bigtable::Table(client_, kTableId);
};

google::bigtable::v2::ReadRowsResponse ReadRowsResponseFromString(
    std::string const& repr);

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_TEST_FIXTURE_H
