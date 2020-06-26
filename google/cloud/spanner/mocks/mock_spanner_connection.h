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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_SPANNER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_SPANNER_CONNECTION_H

#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/read_partition.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
/// Define classes to mock the Cloud Spanner C++ client APIs.
namespace spanner_mocks {
/// An inlined versioned namespace to avoid dependency diamonds.
inline namespace SPANNER_CLIENT_NS {

/**
 * A class to mock `google::cloud::spanner::Connection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors from a `spanner::Client`. To do so, construct a
 * `spanner::Client` with an instance of this class. Then use the Google Test
 * framework functions to program the behavior of this mock.
 *
 * @see @ref spanner-mocking for an example using this class.
 */
class MockConnection : public spanner::Connection {
 public:
  MOCK_METHOD1(Read, spanner::RowStream(ReadParams));
  MOCK_METHOD1(PartitionRead, StatusOr<std::vector<spanner::ReadPartition>>(
                                  PartitionReadParams));
  MOCK_METHOD1(ExecuteQuery, spanner::RowStream(SqlParams));
  MOCK_METHOD1(ExecuteDml, StatusOr<spanner::DmlResult>(SqlParams));
  MOCK_METHOD1(ProfileQuery, spanner::ProfileQueryResult(SqlParams));
  MOCK_METHOD1(ProfileDml, StatusOr<spanner::ProfileDmlResult>(SqlParams));
  MOCK_METHOD1(AnalyzeSql, StatusOr<spanner::ExecutionPlan>(SqlParams));
  MOCK_METHOD1(ExecutePartitionedDml, StatusOr<spanner::PartitionedDmlResult>(
                                          ExecutePartitionedDmlParams));
  MOCK_METHOD1(PartitionQuery, StatusOr<std::vector<spanner::QueryPartition>>(
                                   PartitionQueryParams));
  MOCK_METHOD1(ExecuteBatchDml,
               StatusOr<spanner::BatchDmlResult>(ExecuteBatchDmlParams));
  MOCK_METHOD1(Commit, StatusOr<spanner::CommitResult>(CommitParams));
  MOCK_METHOD1(Rollback, Status(RollbackParams));
};

/**
 * Mock the results of a ExecuteQuery() or Read() operation.
 *
 * @see @ref spanner-mocking for an example using this class.
 */
class MockResultSetSource : public spanner::internal::ResultSourceInterface {
 public:
  MOCK_METHOD0(NextRow, StatusOr<spanner::Row>());
  MOCK_METHOD0(Metadata, optional<google::spanner::v1::ResultSetMetadata>());
  MOCK_CONST_METHOD0(Stats, optional<google::spanner::v1::ResultSetStats>());
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_SPANNER_CONNECTION_H
