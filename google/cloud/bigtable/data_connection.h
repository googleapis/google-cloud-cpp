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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_DATA_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_DATA_CONNECTION_H

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
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<bigtable::FailedMutation> MakeFailedMutations(Status const& status,
                                                          std::size_t n);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Wrap the arguments to `ReadRows()`.
struct ReadRowsParams {
  std::string table_name;
  std::string app_profile_id;
  RowSet row_set;
  std::int64_t rows_limit;
  Filter filter = Filter::PassAllFilter();
  bool reverse = false;
};

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
 *
 * @par Connection Pool
 * This class opens a number of [gRPC Channels] upon its construction. These
 * channels each initiate a connection to the Cloud Bigtable service. This is a
 * relatively slow operation that can take milliseconds, so applications are
 * advised to reuse `DataConnection` objects when possible.
 *
 * The exact number of channels can be configured with the
 * `google::cloud::GrpcNumChannelsOption`. If this option is not set, the class
 * creates between 1 and 64 channels. The specific algorithm and number of
 * channels is an implementation detail, and subject to change without notice.
 *
 * Each request sent from the client library cycles through the channels in the
 * connection pool in a round robin fashion.
 *
 * Unused gRPC channels can enter an idle state. Refreshing them during a
 * request may incur additional latency. To avoid this, the client library keeps
 * connections open by refreshing the channels in a background thread. Advanced
 * users can configure the frequency of the refreshes, or disable this feature,
 * by using `bigtable::MinConnectionRefreshOption` and
 * `bigtable::MaxConnectionRefreshOption`.
 *
 * [gRPC Channels]: https://grpc.io/docs/what-is-grpc/core-concepts/#channels
 */
class DataConnection {
 public:
  virtual ~DataConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual Status Apply(std::string const& table_name, SingleRowMutation mut);

  virtual future<Status> AsyncApply(std::string const& table_name,
                                    SingleRowMutation mut);

  virtual std::vector<FailedMutation> BulkApply(std::string const& table_name,
                                                BulkMutation mut);

  virtual future<std::vector<FailedMutation>> AsyncBulkApply(
      std::string const& table_name, BulkMutation mut);

  /// Prefer to use `ReadRowsFull()` in mocks.
  virtual RowReader ReadRows(std::string const& table_name, RowSet row_set,
                             std::int64_t rows_limit, Filter filter);

  virtual RowReader ReadRowsFull(ReadRowsParams params);

  virtual StatusOr<std::pair<bool, Row>> ReadRow(std::string const& table_name,
                                                 std::string row_key,
                                                 Filter filter);

  virtual StatusOr<MutationBranch> CheckAndMutateRow(
      std::string const& table_name, std::string row_key, Filter filter,
      std::vector<Mutation> true_mutations,
      std::vector<Mutation> false_mutations);

  virtual future<StatusOr<MutationBranch>> AsyncCheckAndMutateRow(
      std::string const& table_name, std::string row_key, Filter filter,
      std::vector<Mutation> true_mutations,
      std::vector<Mutation> false_mutations);

  virtual StatusOr<std::vector<RowKeySample>> SampleRows(
      std::string const& table_name);

  virtual future<StatusOr<std::vector<RowKeySample>>> AsyncSampleRows(
      std::string const& table_name);

  virtual StatusOr<Row> ReadModifyWriteRow(
      google::bigtable::v2::ReadModifyWriteRowRequest request);

  virtual future<StatusOr<Row>> AsyncReadModifyWriteRow(
      google::bigtable::v2::ReadModifyWriteRowRequest request);

  virtual void AsyncReadRows(std::string const& table_name,
                             std::function<future<bool>(Row)> on_row,
                             std::function<void(Status)> on_finish,
                             RowSet row_set, std::int64_t rows_limit,
                             Filter filter);

  virtual future<StatusOr<std::pair<bool, Row>>> AsyncReadRow(
      std::string const& table_name, std::string row_key, Filter filter);
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
 * returned `DataConnection`. Expected options are any of the following options
 * or types in the option lists.
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::UnifiedCredentialsOptionList`
 * - `google::cloud::bigtable::AppProfileIdOption`
 * - `google::cloud::bigtable::ClientOptionsList`
 * - `google::cloud::bigtable::DataPolicyOptionList`
 *
 * @note Unrecognized options will be ignored. To debug issues with options set
 *     `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment and unexpected
 *     options will be logged.
 *
 * @param options (optional) Configure the `DataConnection` created by this
 *     function.
 */
std::shared_ptr<DataConnection> MakeDataConnection(Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_DATA_CONNECTION_H
