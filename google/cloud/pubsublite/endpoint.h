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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_ENDPOINT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_ENDPOINT_H

#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns the appropriate endpoint given a zone name.
 *
 * Given a zone name in ${region}-[a-z] form it returns the appropriate regional
 * endpoint to contact the Pub/Sub Lite service. Use the value returned to
 * initialize the library via `google::cloud::EndpointOption`.
 */
StatusOr<std::string> EndpointFromZone(std::string const& zone_id);

/**
 * Returns the appropriate endpoint given a region name.
 *
 * Given a region name it returns the appropriate regional endpoint to contact
 * the Pub/Sub Lite service. Use the value returned to initialize the library
 * via `google::cloud::EndpointOption`.
 */
StatusOr<std::string> EndpointFromRegion(std::string const& region_id);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_ENDPOINT_H
