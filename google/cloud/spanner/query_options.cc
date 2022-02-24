// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

QueryOptions::QueryOptions(Options const& opts) {
  if (opts.has<QueryOptimizerVersionOption>()) {
    optimizer_version_ = opts.get<QueryOptimizerVersionOption>();
  }
  if (opts.has<QueryOptimizerStatisticsPackageOption>()) {
    optimizer_statistics_package_ =
        opts.get<QueryOptimizerStatisticsPackageOption>();
  }
  if (opts.has<RequestPriorityOption>()) {
    request_priority_ = opts.get<RequestPriorityOption>();
  }
  if (opts.has<RequestTagOption>()) {
    request_tag_ = opts.get<RequestTagOption>();
  }
}

QueryOptions::operator Options() const {
  Options opts;
  if (optimizer_version_) {
    opts.set<QueryOptimizerVersionOption>(*optimizer_version_);
  }
  if (optimizer_statistics_package_) {
    opts.set<QueryOptimizerStatisticsPackageOption>(
        *optimizer_statistics_package_);
  }
  if (request_priority_) opts.set<RequestPriorityOption>(*request_priority_);
  if (request_tag_) opts.set<RequestTagOption>(*request_tag_);
  return opts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
