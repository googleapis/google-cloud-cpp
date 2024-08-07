// Copyright 2024 Google LLC
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

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/bigquery/v2/table.proto

#include "google/cloud/bigquerycontrol/v2/internal/table_option_defaults.h"
#include "google/cloud/bigquerycontrol/v2/table_connection.h"
#include "google/cloud/bigquerycontrol/v2/table_options.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/populate_grpc_options.h"
#include <memory>
#include <utility>

namespace google {
namespace cloud {
namespace bigquerycontrol_v2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {
auto constexpr kBackoffScaling = 2.0;
}  // namespace

Options TableServiceDefaultOptions(Options options) {
  options = internal::PopulateCommonOptions(
      std::move(options), "GOOGLE_CLOUD_CPP_TABLE_SERVICE_ENDPOINT", "",
      "GOOGLE_CLOUD_CPP_TABLE_SERVICE_AUTHORITY", "bigquery.googleapis.com");
  options = internal::PopulateGrpcOptions(std::move(options));
  if (!options.has<bigquerycontrol_v2::TableServiceRetryPolicyOption>()) {
    options.set<bigquerycontrol_v2::TableServiceRetryPolicyOption>(
        bigquerycontrol_v2::TableServiceLimitedTimeRetryPolicy(
            std::chrono::minutes(30))
            .clone());
  }
  if (!options.has<bigquerycontrol_v2::TableServiceBackoffPolicyOption>()) {
    options.set<bigquerycontrol_v2::TableServiceBackoffPolicyOption>(
        ExponentialBackoffPolicy(
            std::chrono::seconds(0), std::chrono::seconds(1),
            std::chrono::minutes(5), kBackoffScaling, kBackoffScaling)
            .clone());
  }
  if (!options.has<bigquerycontrol_v2::
                       TableServiceConnectionIdempotencyPolicyOption>()) {
    options
        .set<bigquerycontrol_v2::TableServiceConnectionIdempotencyPolicyOption>(
            bigquerycontrol_v2::
                MakeDefaultTableServiceConnectionIdempotencyPolicy());
  }

  return options;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquerycontrol_v2_internal
}  // namespace cloud
}  // namespace google
