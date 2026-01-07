// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/table.h"
#include <thread>
#include <type_traits>

namespace btproto = ::google::bigtable::v2;
namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MergeOptions;
using ::google::cloud::internal::OptionsSpan;

namespace {
template <typename Request>
void SetCommonTableOperationRequest(Request& request,
                                    std::string const& app_profile_id,
                                    std::string const& table_name) {
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
}
}  // namespace

static_assert(std::is_copy_assignable<bigtable::Table>::value,
              "bigtable::Table must be CopyAssignable");

Status Table::Apply(SingleRowMutation mut, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->Apply(table_name_, std::move(mut));
}

future<Status> Table::AsyncApply(SingleRowMutation mut, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->AsyncApply(table_name_, std::move(mut));
}

std::vector<FailedMutation> Table::BulkApply(BulkMutation mut, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->BulkApply(table_name_, std::move(mut));
}

future<std::vector<FailedMutation>> Table::AsyncBulkApply(BulkMutation mut,
                                                          Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->AsyncBulkApply(table_name_, std::move(mut));
}

RowReader Table::ReadRows(RowSet row_set, Filter filter, Options opts) {
  return ReadRows(std::move(row_set), RowReader::NO_ROWS_LIMIT,
                  std::move(filter), std::move(opts));
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->ReadRows(table_name_, std::move(row_set), rows_limit,
                               std::move(filter));
}

StatusOr<std::pair<bool, Row>> Table::ReadRow(std::string row_key,
                                              Filter filter, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->ReadRow(table_name_, std::move(row_key),
                              std::move(filter));
}

StatusOr<MutationBranch> Table::CheckAndMutateRow(
    std::string row_key, Filter filter, std::vector<Mutation> true_mutations,
    std::vector<Mutation> false_mutations, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->CheckAndMutateRow(
      table_name_, std::move(row_key), std::move(filter),
      std::move(true_mutations), std::move(false_mutations));
}

future<StatusOr<MutationBranch>> Table::AsyncCheckAndMutateRow(
    std::string row_key, Filter filter, std::vector<Mutation> true_mutations,
    std::vector<Mutation> false_mutations, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->AsyncCheckAndMutateRow(
      table_name_, std::move(row_key), std::move(filter),
      std::move(true_mutations), std::move(false_mutations));
}

// Call the `google.bigtable.v2.Bigtable.SampleRowKeys` RPC until
// successful. When RPC is finished, this function returns the SampleRowKeys
// as a std::vector<>. If the RPC fails, it will keep retrying until the
// policies in effect tell us to stop. Note that each retry must clear the
// samples otherwise the result is an inconsistent set of sample row keys.
StatusOr<std::vector<bigtable::RowKeySample>> Table::SampleRows(Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->SampleRows(table_name_);
}

future<StatusOr<std::vector<bigtable::RowKeySample>>> Table::AsyncSampleRows(
    Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->AsyncSampleRows(table_name_);
}

StatusOr<Row> Table::ReadModifyWriteRowImpl(
    btproto::ReadModifyWriteRowRequest request, Options opts) {
  SetCommonTableOperationRequest<
      ::google::bigtable::v2::ReadModifyWriteRowRequest>(
      request, app_profile_id(), table_name_);
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->ReadModifyWriteRow(std::move(request));
}

future<StatusOr<Row>> Table::AsyncReadModifyWriteRowImpl(
    ::google::bigtable::v2::ReadModifyWriteRowRequest request, Options opts) {
  SetCommonTableOperationRequest<
      ::google::bigtable::v2::ReadModifyWriteRowRequest>(
      request, app_profile_id(), table_name_);
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->AsyncReadModifyWriteRow(std::move(request));
}

future<StatusOr<std::pair<bool, Row>>> Table::AsyncReadRow(std::string row_key,
                                                           Filter filter,
                                                           Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), options_));
  return connection_->AsyncReadRow(table_name_, std::move(row_key),
                                   std::move(filter));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
