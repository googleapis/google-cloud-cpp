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
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/row_reader_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::vector<FailedMutation> MakeUnimplementedFailedMutations(std::size_t n) {
  std::vector<FailedMutation> mutations;
  mutations.reserve(n);
  for (int i = 0; i != static_cast<int>(n); ++i) {
    mutations.emplace_back(FailedMutation(
        Status(StatusCode::kUnimplemented, "not implemented"), i));
  }
  return mutations;
}

}  // namespace

DataConnection::~DataConnection() = default;

Status DataConnection::Apply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, SingleRowMutation) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<Status> DataConnection::AsyncApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, SingleRowMutation) {
  return make_ready_future(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

std::vector<FailedMutation> DataConnection::BulkApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, BulkMutation mut) {
  return MakeUnimplementedFailedMutations(mut.size());
}

future<std::vector<FailedMutation>> DataConnection::AsyncBulkApply(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, BulkMutation mut) {
  return make_ready_future(MakeUnimplementedFailedMutations(mut.size()));
}

RowReader DataConnection::ReadRows(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, RowSet, std::int64_t, Filter) {
  return MakeRowReader(std::make_shared<bigtable_internal::StatusOnlyRowReader>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

StatusOr<std::pair<bool, Row>> DataConnection::ReadRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, std::string, Filter) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<MutationBranch> DataConnection::CheckAndMutateRow(
    std::string const&, std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string, Filter, std::vector<Mutation>, std::vector<Mutation>) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<MutationBranch>> DataConnection::AsyncCheckAndMutateRow(
    std::string const&, std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string, Filter, std::vector<Mutation>, std::vector<Mutation>) {
  return make_ready_future<StatusOr<MutationBranch>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

StatusOr<std::vector<RowKeySample>> DataConnection::SampleRows(
    std::string const&, std::string const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

future<StatusOr<std::vector<RowKeySample>>> DataConnection::AsyncSampleRows(
    std::string const&, std::string const&) {
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
    std::string const&, std::string const&,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::function<future<bool>(Row)>, std::function<void(Status)> on_finish,
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    RowSet, std::int64_t, Filter) {
  on_finish(Status(StatusCode::kUnimplemented, "not implemented"));
}

future<StatusOr<std::pair<bool, Row>>> DataConnection::AsyncReadRow(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    std::string const&, std::string const&, std::string, Filter) {
  return make_ready_future<StatusOr<std::pair<bool, Row>>>(
      Status(StatusCode::kUnimplemented, "not implemented"));
}

std::shared_ptr<DataConnection> MakeDataConnection(Options options) {
  google::cloud::internal::CheckExpectedOptions<
      CommonOptionList, GrpcOptionList, DataPolicyOptionList>(options,
                                                              __func__);
  options = bigtable::internal::DefaultDataOptions(std::move(options));
  auto background =
      google::cloud::internal::MakeBackgroundThreadsFactory(options)();
  auto stub = bigtable_internal::CreateBigtableStub(background->cq(), options);
  return std::make_shared<bigtable_internal::DataConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<bigtable::DataConnection> MakeDataConnection(
    std::shared_ptr<BigtableStub> stub, Options options) {
  options = bigtable::internal::DefaultDataOptions(std::move(options));
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<DataConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
