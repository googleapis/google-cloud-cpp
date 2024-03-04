// Copyright 2023 Google LLC
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

#include "google/cloud/bigquery/v2/minimal/internal/table_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::unique_ptr<TableRetryPolicy> retry_policy(Options const& options) {
  return options.get<TableRetryPolicyOption>()->clone();
}

std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<TableBackoffPolicyOption>()->clone();
}

std::unique_ptr<TableIdempotencyPolicy> idempotency_policy(
    Options const& options) {
  return options.get<TableIdempotencyPolicyOption>()->clone();
}

}  // namespace

TableRestConnectionImpl::TableRestConnectionImpl(
    std::shared_ptr<TableRestStub> stub, Options options)
    : stub_(std::move(stub)),
      options_(internal::MergeOptions(std::move(options),
                                      TableConnection::options())) {}

StatusOr<Table> TableRestConnectionImpl::GetTable(
    GetTableRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto result = rest_internal::RestRetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->GetTable(request),
      [this](rest_internal::RestContext& rest_context, Options const&,
             GetTableRequest const& request) {
        return stub_->GetTable(rest_context, request);
      },
      *current, request, __func__);
  if (!result) return std::move(result).status();
  return result->table;
}

StreamRange<ListFormatTable> TableRestConnectionImpl::ListTables(
    ListTablesRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto req = request;
  req.set_page_token("");

  auto retry = std::shared_ptr<TableRetryPolicy const>(retry_policy(*current));
  auto backoff = std::shared_ptr<BackoffPolicy const>(backoff_policy(*current));
  auto idempotency = idempotency_policy(*current)->ListTables(req);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<
      StreamRange<ListFormatTable>>(
      std::move(current), std::move(req),
      [stub = stub_, retry = std::move(retry), backoff = std::move(backoff),
       idempotency,
       function_name](Options const& options, ListTablesRequest const& r) {
        return rest_internal::RestRetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](rest_internal::RestContext& context, Options const&,
                   ListTablesRequest const& request) {
              return stub->ListTables(context, request);
            },
            options, r, function_name);
      },
      [](ListTablesResponse r) {
        std::vector<ListFormatTable> result(r.tables.size());
        auto& messages = r.tables;
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
