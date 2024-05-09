// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/internal/cloud_zone.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<CloudZone> MakeCloudZone(std::string const& zone) {
  std::vector<std::string> splits = absl::StrSplit(zone, '-');
  if (splits.size() != 3 || splits[2].length() != 1) {
    return internal::InvalidArgumentError("Invalid zone name",
                                          GCP_ERROR_INFO());
  }
  return CloudZone{CloudRegion{absl::StrCat(splits[0], "-", splits[1])},
                   splits[2][0]};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
