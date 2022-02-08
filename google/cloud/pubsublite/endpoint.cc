// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/endpoint.h"
#include <cctype>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::string> EndpointFromZone(std::string const& zone_id) {
  auto const n = zone_id.size();
  if (n < 3 || !std::isalpha(zone_id[n - 1]) || zone_id[n - 2] != '-') {
    return Status(StatusCode::kInvalidArgument,
                  "expected a zone id in <region>-[a-z] format");
  }
  return EndpointFromRegion(zone_id.substr(0, n - 2));
}

StatusOr<std::string> EndpointFromRegion(std::string const& region_id) {
  auto const n = region_id.size();
  // These are not all the constraints in a region id. Typically, they are in
  // the form <geo>-<direction><digit>. Full validation would require contacting
  // a source of truth, seems like overkill for this application.
  if (n < 2 || !std::isalpha(region_id.front()) ||
      !std::isdigit(region_id.back())) {
    return Status(
        StatusCode::kInvalidArgument,
        "region ids start with a alphabetic character and end with a digit");
  }
  return region_id + "-pubsublite.googleapis.com";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
