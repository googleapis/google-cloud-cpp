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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DATA_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DATA_CONNECTION_H

#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/mutation_branch.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/row_key_sample.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
// TODO(#8860) - Make this class public, when it is ready.
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A connection to the Cloud Bigtable Data API.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `Table`. This allows users to inject custom behavior (e.g.,
 * with a Google Mock object) in a `Table` object for use in their own tests.
 *
 * To create a concrete instance, see `MakeDataConnection()`.
 *
 * For mocking, see `bigtable_mocks::MockDataConnection`.
 */
class DataConnection {
 public:
  virtual ~DataConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual Status Apply(std::string const& app_profile_id,
                       std::string const& table_name,
                       bigtable::SingleRowMutation mut);

  virtual future<Status> AsyncApply(std::string const& app_profile_id,
                                    std::string const& table_name,
                                    bigtable::SingleRowMutation mut);

  virtual std::vector<bigtable::FailedMutation> BulkApply(
      std::string const& app_profile_id, std::string const& table_name,
      bigtable::BulkMutation mut);

  virtual future<std::vector<bigtable::FailedMutation>> AsyncBulkApply(
      std::string const& app_profile_id, std::string const& table_name,
      bigtable::BulkMutation mut);

  virtual bigtable::RowReader ReadRows(std::string const& app_profile_id,
                                       std::string const& table_name,
                                       bigtable::RowSet row_set,
                                       std::int64_t rows_limit,
                                       bigtable::Filter filter);

  virtual StatusOr<std::pair<bool, bigtable::Row>> ReadRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter);

  virtual StatusOr<bigtable::MutationBranch> CheckAndMutateRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter,
      std::vector<bigtable::Mutation> true_mutations,
      std::vector<bigtable::Mutation> false_mutations);

  virtual future<StatusOr<bigtable::MutationBranch>> AsyncCheckAndMutateRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter,
      std::vector<bigtable::Mutation> true_mutations,
      std::vector<bigtable::Mutation> false_mutations);

  virtual StatusOr<std::vector<bigtable::RowKeySample>> SampleRows(
      std::string const& app_profile_id, std::string const& table_name);

  virtual future<StatusOr<std::vector<bigtable::RowKeySample>>> AsyncSampleRows(
      std::string const& app_profile_id, std::string const& table_name);

  virtual StatusOr<bigtable::Row> ReadModifyWriteRow(
      google::bigtable::v2::MutateRowRequest request);

  virtual future<StatusOr<bigtable::Row>> AsyncReadModifyWriteRow(
      google::bigtable::v2::MutateRowRequest request);

  virtual void AsyncReadRows(std::string const& app_profile_id,
                             std::string const& table_name,
                             std::function<future<bool>(bigtable::Row)> on_row,
                             std::function<void(Status)> on_finish,
                             bigtable::RowSet row_set, std::int64_t rows_limit,
                             bigtable::Filter filter);

  virtual future<StatusOr<std::pair<bool, bigtable::Row>>> AsyncReadRow(
      std::string const& app_profile_id, std::string const& table_name,
      std::string row_key, bigtable::Filter filter);
};

/**
 * Returns a `DataConnection` object that can be used for interacting with
 * the Cloud Bigtable Data API.
 *
 * The returned connection object should not be used directly; instead it
 * should be given to a `Table` instance, and methods should be invoked on
 * `Table`.
 *
 * The optional @p opts argument may be used to configure aspects of the
 * returned `DataConnection`. Expected options are any of the types in the
 * following option lists.
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::bigtable::ClientOptionsList`
 * - `google::cloud::bigtable::DataPolicyOptionList`
 *
 * @note Unrecognized options will be ignored. To debug issues with options set
 *     `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment and unexpected
 *     options will be logged.
 *
 * @param opts (optional) Configure the `DataConnection` created by this
 * function.
 */
std::shared_ptr<DataConnection> MakeDataConnection(Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<DataConnection> MakeDataConnection(
    std::shared_ptr<BigtableStub> stub, Options options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DATA_CONNECTION_H
