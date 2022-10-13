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
  auto make_error = [] {
    return Status(StatusCode::kInvalidArgument,
                  "expected a zone id in <region>-[a-z] format");
  };
  auto const n = zone_id.size();
  if (n < 3) return make_error();
  auto back = static_cast<unsigned char>(zone_id.back());
  if (!std::isalpha(back) || zone_id[n - 2] != '-') return make_error();
  return EndpointFromRegion(zone_id.substr(0, n - 2));
}

StatusOr<std::string> EndpointFromRegion(std::string const& region_id) {
  auto make_error = [] {
    return Status(
        StatusCode::kInvalidArgument,
        "region ids start with a alphabetic character and end with a digit");
  };
  if (region_id.size() < 2) return make_error();
  auto front = static_cast<unsigned char>(region_id.front());
  auto back = static_cast<unsigned char>(region_id.back());
  // These are not all the constraints in a region id. Typically, they are in
  // the form <geo>-<direction><digit>. Full validation would require contacting
  // a source of truth, seems like overkill for this application.
  if (!std::isalpha(front) || !std::isdigit(back)) return make_error();
  return region_id + "-pubsublite.googleapis.com";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google
