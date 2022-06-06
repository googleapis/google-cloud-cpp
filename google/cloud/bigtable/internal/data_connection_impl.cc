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

#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/default_row_reader.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/retry_loop.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bigtable::Row TransformReadModifyWriteRowResponse(
    google::bigtable::v2::ReadModifyWriteRowResponse response) {
  std::vector<bigtable::Cell> cells;
  auto& row = *response.mutable_row();
  for (auto& family : *row.mutable_families()) {
    for (auto& column : *family.mutable_columns()) {
      for (auto& cell : *column.mutable_cells()) {
        std::vector<std::string> labels;
        std::move(cell.mutable_labels()->begin(), cell.mutable_labels()->end(),
                  std::back_inserter(labels));
        cells.emplace_back(row.key(), family.name(), column.qualifier(),
                           cell.timestamp_micros(),
                           std::move(*cell.mutable_value()), std::move(labels));
      }
    }
  }

  return bigtable::Row(std::move(*row.mutable_key()), std::move(cells));
}

DataConnectionImpl::DataConnectionImpl(
    std::unique_ptr<BackgroundThreads> background,
    std::shared_ptr<BigtableStub> stub, Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      options_(internal::MergeOptions(
          std::move(options),
          bigtable::internal::DefaultDataOptions(DataConnection::options()))) {}

Status DataConnectionImpl::Apply(std::string const& app_profile_id,
                                 std::string const& table_name,
                                 bigtable::SingleRowMutation mut) {
  google::bigtable::v2::MutateRowRequest request;
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
  mut.MoveTo(request);

  auto idempotent_policy = idempotency_policy();
  bool const is_idempotent = std::all_of(
      request.mutations().begin(), request.mutations().end(),
      [&idempotent_policy](google::bigtable::v2::Mutation const& m) {
        return idempotent_policy->is_idempotent(m);
      });

  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(), backoff_policy(),
      is_idempotent ? Idempotency::kIdempotent : Idempotency::kNonIdempotent,
      [this](grpc::ClientContext& context,
             google::bigtable::v2::MutateRowRequest const& request) {
        return stub_->MutateRow(context, request);
      },
      request, __func__);
  if (!sor) return std::move(sor).status();
  return Status{};
}

std::vector<bigtable::FailedMutation> DataConnectionImpl::BulkApply(
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::BulkMutation mut) {
  if (mut.empty()) return {};
  bigtable::internal::BulkMutator mutator(
      app_profile_id, table_name, *idempotency_policy(), std::move(mut));
  // We wait to allocate the policies until they are needed as a
  // micro-optimization.
  std::unique_ptr<DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  do {
    auto status = mutator.MakeOneRequest(*stub_);
    if (status.ok()) continue;
    if (!retry) retry = retry_policy();
    if (!retry->OnFailure(status)) break;
    if (!backoff) backoff = backoff_policy();
    auto delay = backoff->OnCompletion();
    std::this_thread::sleep_for(delay);
  } while (mutator.HasPendingMutations());
  return std::move(mutator).OnRetryDone();
}

bigtable::RowReader DataConnectionImpl::ReadRows(
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::RowSet row_set, std::int64_t rows_limit,
    bigtable::Filter filter) {
  auto impl = std::make_shared<DefaultRowReader>(
      stub_, app_profile_id, table_name, std::move(row_set), rows_limit,
      std::move(filter), retry_policy(), backoff_policy());
  return MakeRowReader(std::move(impl));
}

StatusOr<std::pair<bool, bigtable::Row>> DataConnectionImpl::ReadRow(
    std::string const& app_profile_id, std::string const& table_name,
    std::string row_key, bigtable::Filter filter) {
  bigtable::RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  auto reader = ReadRows(app_profile_id, table_name, std::move(row_set),
                         rows_limit, std::move(filter));

  auto it = reader.begin();
  if (it == reader.end()) return std::make_pair(false, bigtable::Row("", {}));
  if (!*it) return it->status();
  auto result = std::make_pair(true, std::move(**it));
  if (++it != reader.end()) {
    return Status(
        StatusCode::kInternal,
        "internal error - RowReader returned more than one row in ReadRow()");
  }
  return result;
}

StatusOr<bigtable::Row> DataConnectionImpl::ReadModifyWriteRow(
    google::bigtable::v2::ReadModifyWriteRowRequest request) {
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(), backoff_policy(), Idempotency::kNonIdempotent,
      [this](grpc::ClientContext& context,
             google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
        return stub_->ReadModifyWriteRow(context, request);
      },
      request, __func__);
  if (!sor) return std::move(sor).status();
  return TransformReadModifyWriteRowResponse(*std::move(sor));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
