// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ClientOptions::operator Options() const {
  Options opts;
  auto optimizer_version = query_options_.optimizer_version();
  if (optimizer_version) {
    opts.set<RequestOptimizerVersionOption>(*optimizer_version);
  }
  auto optimizer_statistics_package =
      query_options_.optimizer_statistics_package();
  if (optimizer_statistics_package) {
    opts.set<RequestOptimizerStatisticsPackageOption>(
        *optimizer_statistics_package);
  }
  auto request_priority = query_options_.request_priority();
  if (request_priority) {
    opts.set<RequestPriorityOption>(*request_priority);
  }
  auto request_tag = query_options_.request_tag();
  if (request_tag) {
    opts.set<RequestTagOption>(*request_tag);
  }
  return opts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
