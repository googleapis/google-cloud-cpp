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

#include "bigtable/client/table.h"
#include "bigtable/client/internal/bulk_mutator.h"
#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/internal/unary_rpc_utils.h"
#include <thread>

namespace btproto = ::google::bigtable::v2;

namespace {
[[noreturn]] void ReportPermanentFailures(
    char const* msg, grpc::Status const& status,
    std::vector<bigtable::FailedMutation> failures) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  throw bigtable::PermanentMutationFailure(msg, status, std::move(failures));
#else
  std::cerr << msg << "\n"
            << "Status: " << status.error_message() << " ["
            << status.error_code() << "] - " << status.error_details()
            << std::endl;
  for (auto const& failed : failures) {
    std::cerr << "Mutation " << failed.original_index() << " failed with"
              << failed.status().error_message() << " ["
              << failed.status().error_code() << "]" << std::endl;
  }
  std::cerr << "Aborting because exceptions are disabled." << std::endl;
  std::abort();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
}  // namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
void Table::Apply(SingleRowMutation&& mut) {
  std::vector<FailedMutation> failures = impl_.Apply(std::move(mut));
  if (not failures.empty()) {
    grpc::Status status = failures.front().status();
    ReportPermanentFailures(status.error_message().c_str(), status, failures);
  }
}

void Table::BulkApply(BulkMutation&& mut) {
  grpc::Status status;
  std::vector<FailedMutation> failures =
      impl_.BulkApply(std::move(mut), status);
  if (not status.ok()) {
    ReportPermanentFailures(status.error_message().c_str(), status, failures);
  }
}

RowReader Table::ReadRows(RowSet row_set, Filter filter) {
  return impl_.ReadRows(std::move(row_set), std::move(filter), true);
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter) {
  return impl_.ReadRows(std::move(row_set), rows_limit, std::move(filter),
                        true);
}

std::pair<bool, Row> Table::ReadRow(std::string row_key, Filter filter) {
  grpc::Status status;
  auto result = impl_.ReadRow(std::move(row_key), std::move(filter), status);
  if (not status.ok()) {
    internal::RaiseRuntimeError(status.error_message());
  }
  return result;
}

bool Table::CheckAndMutateRow(std::string row_key, Filter filter,
                              std::vector<Mutation> true_mutations,
                              std::vector<Mutation> false_mutations) {
  grpc::Status status;
  bool value = impl_.CheckAndMutateRow(std::move(row_key), std::move(filter),
                                       std::move(true_mutations),
                                       std::move(false_mutations), status);
  if (not status.ok()) {
    internal::RaiseRpcError(status, status.error_message());
  }
  return value;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
