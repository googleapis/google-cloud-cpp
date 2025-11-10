// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/client.h"
#include "google/cloud/options.h"
#include "internal/partial_result_set_source.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MergeOptions;
using ::google::cloud::internal::OptionsSpan;

StatusOr<PreparedQuery> Client::PrepareQuery(InstanceResource const& instance,
                                             SqlStatement const& statement,
                                             Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), opts_));
  return conn_->PrepareQuery({std::move(instance), std::move(statement)});
}

future<StatusOr<PreparedQuery>> Client::AsyncPrepareQuery(
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    InstanceResource const& instance, SqlStatement const& statement, Options) {
  return conn_->AsyncPrepareQuery({std::move(instance), std::move(statement)});
}

RowStream Client::ExecuteQuery(BoundQuery&& bound_query, Options opts) {
  OptionsSpan span(MergeOptions(std::move(opts), opts_));
  return conn_->ExecuteQuery({std::move(bound_query)});
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
