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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/internal/async_future_from_callback.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include <thread>
#include <type_traits>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
static_assert(std::is_copy_assignable<bigtable::Table>::value,
              "bigtable::Table must be CopyAssignable");

Status Table::Apply(SingleRowMutation mut) {
  std::vector<FailedMutation> failures = impl_.Apply(std::move(mut));
  if (!failures.empty()) {
    return failures.front().status();
  }
  return google::cloud::Status{};
}

future<void> Table::AsyncApply(SingleRowMutation mut, CompletionQueue& cq) {
  promise<google::bigtable::v2::MutateRowResponse> p;
  future<google::bigtable::v2::MutateRowResponse> result = p.get_future();

  impl_.AsyncApply(
      cq, internal::MakeAsyncFutureFromCallback(std::move(p), "AsyncApply"),
      std::move(mut));

  auto final =
      result.then([](future<google::bigtable::v2::MutateRowResponse> f) {
        auto mutate_row_response = f.get();
      });

  return final;
}

std::vector<FailedMutation> Table::BulkApply(BulkMutation mut) {
  grpc::Status status;

  // We do not no need to check status.ok() anymore as
  // FailedMutation class has now a google::cloud::Status member
  // which can be accessed via status()
  std::vector<FailedMutation> failures =
      impl_.BulkApply(std::move(mut), status);

  return failures;
}

future<void> Table::AsyncBulkApply(BulkMutation mut, CompletionQueue& cq) {
  promise<std::vector<FailedMutation>> pfm;
  future<std::vector<FailedMutation>> resultfm = pfm.get_future();
  impl_.AsyncBulkApply(
      cq,
      internal::MakeAsyncFutureFromCallback(std::move(pfm), "AsyncBulkApply"),
      std::move(mut));

  auto final = resultfm.then([](future<std::vector<FailedMutation>> f) {
    auto failures = f.get();

    for (auto const& failed : failures) {
      std::cerr << "Mutation " << failed.original_index() << " failed with"
                << failed.status().message() << " [" << failed.status().code()
                << "]\n";
    }
  });

  return final;
}

RowReader Table::ReadRows(RowSet row_set, Filter filter) {
  return impl_.ReadRows(std::move(row_set), std::move(filter));
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter) {
  return impl_.ReadRows(std::move(row_set), rows_limit, std::move(filter));
}

StatusOr<std::pair<bool, Row>> Table::ReadRow(std::string row_key,
                                              Filter filter) {
  Status status;
  auto result = impl_.ReadRow(std::move(row_key), std::move(filter), status);
  if (!status.ok()) {
    return status;
  }
  return result;
}

StatusOr<bool> Table::CheckAndMutateRow(std::string row_key, Filter filter,
                                        std::vector<Mutation> true_mutations,
                                        std::vector<Mutation> false_mutations) {
  grpc::Status status;
  bool value = impl_.CheckAndMutateRow(std::move(row_key), std::move(filter),
                                       std::move(true_mutations),
                                       std::move(false_mutations), status);
  if (!status.ok()) {
    return bigtable::internal::MakeStatusFromRpcError(status);
  }
  return value;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
