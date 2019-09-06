// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_CONNECTION_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_CONNECTION_H_

#include "google/cloud/spanner/connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
/**
 * Contains all the types and functions to help test
 */
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

class MockConnection : public spanner::Connection {
 public:
  MOCK_METHOD1(Read, StatusOr<spanner::ResultSet>(ReadParams));
  MOCK_METHOD1(PartitionRead, StatusOr<std::vector<spanner::ReadPartition>>(
                                  PartitionReadParams));
  MOCK_METHOD1(ExecuteSql, StatusOr<spanner::ResultSet>(ExecuteSqlParams));
  MOCK_METHOD1(ExecutePartitionedDml, StatusOr<spanner::PartitionedDmlResult>(
                                          ExecutePartitionedDmlParams));
  MOCK_METHOD1(PartitionQuery, StatusOr<std::vector<spanner::QueryPartition>>(
                                   PartitionQueryParams));
  MOCK_METHOD1(ExecuteBatchDml,
               StatusOr<spanner::BatchDmlResult>(BatchDmlParams));
  MOCK_METHOD1(Commit, StatusOr<spanner::CommitResult>(CommitParams));
  MOCK_METHOD1(Rollback, Status(RollbackParams));
};

class MockResultSetSource : public spanner::internal::ResultSetSource {
 public:
  MOCK_METHOD0(NextValue, StatusOr<optional<spanner::Value>>());
  MOCK_METHOD0(Metadata, optional<google::spanner::v1::ResultSetMetadata>());
  MOCK_METHOD0(Stats, optional<google::spanner::v1::ResultSetStats>());
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_CONNECTION_H_
