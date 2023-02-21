// Copyright 2023 Google LLC
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

#include "google/cloud/spanner/internal/route_to_leader.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void RouteToLeader(grpc::ClientContext& context) {
  auto const& options = internal::CurrentOptions();
  if (!options.has<spanner::RouteToLeaderOption>() ||
      options.get<spanner::RouteToLeaderOption>()) {
    context.AddMetadata("x-goog-spanner-route-to-leader", "true");
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
