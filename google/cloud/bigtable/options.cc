// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include <chrono>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Options MergeOptions(Options preferred, Options alternatives) {
  return MergeOptions(std::move(preferred), std::move(alternatives),
                      [] { return std::chrono::system_clock::now(); });
}

Options MergeOptions(
    Options preferred, Options alternatives,
    std::function<std::chrono::system_clock::time_point()> now_fn) {
  auto merged_options =
      internal::MergeOptions(std::move(preferred), std::move(alternatives));

  if (merged_options.has<bigtable::DeadlineOption>()) {
    auto deadline = merged_options.get<bigtable::DeadlineOption>();
    auto existing_fn =
        internal::ExtractOption<internal::GrpcSetupOption>(merged_options);
    if (existing_fn.has_value()) {
      auto apply_deadline = [existing_fn = *std::move(existing_fn),
                             now_fn = std::move(now_fn),
                             deadline](grpc::ClientContext& context) {
        existing_fn(context);
        context.set_deadline(now_fn() + deadline);
      };
      merged_options.set<internal::GrpcSetupOption>(apply_deadline);
    } else {
      auto apply_deadline = [now_fn = std::move(now_fn),
                             deadline](grpc::ClientContext& context) {
        context.set_deadline(now_fn() + deadline);
      };
      merged_options.set<internal::GrpcSetupOption>(apply_deadline);
    }
  }
  return merged_options;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
