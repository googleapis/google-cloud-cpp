// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_DATA_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_DATA_CONNECTION_H

#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_mocks {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
// TODO(#8860) - Remove the internal namespace
namespace internal {

/**
 * A class to mock `google::cloud::bigtable::DataConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors from a `bigtable::Table`. To do so, construct a
 * `bigtable::Table` with an instance of this class. Then use the Google Test
 * framework functions to program the behavior of this mock.
 */
class MockDataConnection : public bigtable::DataConnection {
 public:
  MOCK_METHOD(Options, options, (), (override));

  MOCK_METHOD(Status, Apply,
              (std::string const& app_profile_id, std::string const& table_name,
               bigtable::SingleRowMutation mut),
              (override));

  MOCK_METHOD(future<Status>, AsyncApply,
              (std::string const& app_profile_id, std::string const& table_name,
               bigtable::SingleRowMutation mut),
              (override));

  MOCK_METHOD(std::vector<bigtable::FailedMutation>, BulkApply,
              (std::string const& app_profile_id, std::string const& table_name,
               bigtable::BulkMutation mut),
              (override));

  MOCK_METHOD(future<std::vector<bigtable::FailedMutation>>, AsyncBulkApply,
              (std::string const& app_profile_id, std::string const& table_name,
               bigtable::BulkMutation mut),
              (override));

  MOCK_METHOD(bigtable::RowReader, ReadRows,
              (std::string const& app_profile_id, std::string const& table_name,
               bigtable::RowSet row_set, std::int64_t rows_limit,
               bigtable::Filter filter),
              (override));

  MOCK_METHOD((StatusOr<std::pair<bool, bigtable::Row>>), ReadRow,
              (std::string const& app_profile_id, std::string const& table_name,
               std::string row_key, bigtable::Filter filter),
              (override));

  MOCK_METHOD(StatusOr<bigtable::MutationBranch>, CheckAndMutateRow,
              (std::string const& app_profile_id, std::string const& table_name,
               std::string row_key, bigtable::Filter filter,
               std::vector<bigtable::Mutation> true_mutations,
               std::vector<bigtable::Mutation> false_mutations),
              (override));

  MOCK_METHOD(future<StatusOr<bigtable::MutationBranch>>,
              AsyncCheckAndMutateRow,
              (std::string const& app_profile_id, std::string const& table_name,
               std::string row_key, bigtable::Filter filter,
               std::vector<bigtable::Mutation> true_mutations,
               std::vector<bigtable::Mutation> false_mutations),
              (override));

  MOCK_METHOD(StatusOr<std::vector<bigtable::RowKeySample>>, SampleRows,
              (std::string const& app_profile_id,
               std::string const& table_name),
              (override));

  MOCK_METHOD(future<StatusOr<std::vector<bigtable::RowKeySample>>>,
              AsyncSampleRows,
              (std::string const& app_profile_id,
               std::string const& table_name),
              (override));

  MOCK_METHOD(StatusOr<bigtable::Row>, ReadModifyWriteRow,
              (google::bigtable::v2::ReadModifyWriteRowRequest request),
              (override));

  MOCK_METHOD(future<StatusOr<bigtable::Row>>, AsyncReadModifyWriteRow,
              (google::bigtable::v2::ReadModifyWriteRowRequest request),
              (override));

  MOCK_METHOD(void, AsyncReadRows,
              (std::string const& app_profile_id, std::string const& table_name,
               std::function<future<bool>(bigtable::Row)> on_row,
               std::function<void(Status)> on_finish, bigtable::RowSet row_set,
               std::int64_t rows_limit, bigtable::Filter filter),
              (override));

  MOCK_METHOD((future<StatusOr<std::pair<bool, bigtable::Row>>>), AsyncReadRow,
              (std::string const& app_profile_id, std::string const& table_name,
               std::string row_key, bigtable::Filter filter),
              (override));
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MOCKS_MOCK_DATA_CONNECTION_H
