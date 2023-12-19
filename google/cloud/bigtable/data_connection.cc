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

#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/internal/data_tracing_connection.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/cloud/bigtable/internal/row_reader_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::vector<bigtable::FailedMutation> MakeFailedMutations(Status const& status,
                                                          std::size_t n) {
  std::vector<bigtable::FailedMutation> mutations;
  mutations.reserve(n);
  for (int i = 0; i != static_cast<int>(n); ++i) {
    mutations.emplace_back(status, i);
  }
  return mutations;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DataConnection::~DataConnection() = default;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status DataConnection::Apply(std::string const&, SingleRowMutation) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<Status> DataConnection::AsyncApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, SingleRowMutation) {
  return make_ready_future(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

std::vector<FailedMutation> DataConnection::BulkApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, BulkMutation mut) {
  return bigtable_internal::MakeFailedMutations(
      Status(StatusCode::kUnimplemented, "not-implemented"), mut.size());
}

future<std::vector<FailedMutation>> DataConnection::AsyncBulkApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, BulkMutation mut) {
  return make_ready_future(bigtable_internal::MakeFailedMutations(
      Status(StatusCode::kUnimplemented, "not-implemented"), mut.size()));
}

RowReader DataConnection::ReadRows(std::string const& table_name,
                                   RowSet row_set, std::int64_t rows_limit,
                                   Filter filter) {
  auto const& options = google::cloud::internal::CurrentOptions();
  return ReadRowsFull(ReadRowsParams{
      std::move(table_name),
      options.get<AppProfileIdOption>(),
      std::move(row_set),
      rows_limit,
      std::move(filter),
      options.get<ReverseScanOption>(),
  });
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
RowReader DataConnection::ReadRowsFull(ReadRowsParams) {
  return MakeRowReader(std::make_shared<bigtable_internal::StatusOnlyRowReader>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

StatusOr<std::pair<bool, Row>> DataConnection::ReadRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string, Filter) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<MutationBranch> DataConnection::CheckAndMutateRow(
    std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string, Filter, std::vector<Mutation>, std::vector<Mutation>) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<MutationBranch>> DataConnection::AsyncCheckAndMutateRow(
    std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string, Filter, std::vector<Mutation>, std::vector<Mutation>) {
  return make_ready_future<StatusOr<MutationBranch>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

StatusOr<std::vector<RowKeySample>> DataConnection::SampleRows(
    std::string const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<std::vector<RowKeySample>>> DataConnection::AsyncSampleRows(
    std::string const&) {
  return make_ready_future<StatusOr<std::vector<RowKeySample>>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

StatusOr<Row> DataConnection::ReadModifyWriteRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    google::bigtable::v2::ReadModifyWriteRowRequest) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<Row>> DataConnection::AsyncReadModifyWriteRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    google::bigtable::v2::ReadModifyWriteRowRequest) {
  return make_ready_future<StatusOr<Row>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

void DataConnection::AsyncReadRows(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::function<future<bool>(Row)>,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::function<void(Status)> on_finish, RowSet, std::int64_t, Filter) {
  on_finish(Status(StatusCode::kUnimplemented, "not implemented"));
}

future<StatusOr<std::pair<bool, Row>>> DataConnection::AsyncReadRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string, Filter) {
  return make_ready_future<StatusOr<std::pair<bool, Row>>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

std::shared_ptr<DataConnection> MakeDataConnection(Options options) {
  google::cloud::internal::CheckExpectedOptions<
      AppProfileIdOption, CommonOptionList, GrpcOptionList,
      UnifiedCredentialsOptionList, ClientOptionList, DataPolicyOptionList>(
      options, __func__);
  options = bigtable::internal::DefaultDataOptions(std::move(options));
  auto background =
      google::cloud::internal::MakeBackgroundThreadsFactory(options)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      background->cq(), options);
  auto stub = bigtable_internal::CreateBigtableStub(std::move(auth),
                                                    background->cq(), options);
  auto limiter =
      bigtable_internal::MakeMutateRowsLimiter(background->cq(), options);
  std::shared_ptr<DataConnection> conn =
      std::make_shared<bigtable_internal::DataConnectionImpl>(
          std::move(background), std::move(stub), std::move(limiter),
          std::move(options));
  if (google::cloud::internal::TracingEnabled(conn->options())) {
    conn = bigtable_internal::MakeDataTracingConnection(std::move(conn));
  }
  return conn;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
