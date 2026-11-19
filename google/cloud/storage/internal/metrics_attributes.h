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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METRICS_ATTRIBUTES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METRICS_ATTRIBUTES_H

#include "google/cloud/version.h"
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Normalizes a configured endpoint into the `server.address` attribute value
/// by stripping URI schemes, paths/queries, IPv6 brackets, and trailing ports.
std::string NormalizeServerAddress(std::string_view endpoint);

/// Classifies a gRPC peer URI or IP address into `"directpath"`,
/// `"cloudpath"`, or `"unknown"` for the `network.path` attribute.
std::string_view NetworkPathOfPeer(std::string_view peer);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METRICS_ATTRIBUTES_H
