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

#include "google/cloud/storage/internal/tracked_features.h"
#include "google/cloud/internal/base64_transforms.h"
#include <array>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::uint32_t FeatureBit(TrackedFeature feature) {
  return std::uint32_t{1} << static_cast<std::uint32_t>(feature);
}

std::string EncodeFeatureMask(std::uint32_t mask) {
  if (mask == 0) return {};
  std::array<std::uint8_t, 4> const bytes{
      static_cast<std::uint8_t>((mask >> 24) & 0xFF),
      static_cast<std::uint8_t>((mask >> 16) & 0xFF),
      static_cast<std::uint8_t>((mask >> 8) & 0xFF),
      static_cast<std::uint8_t>(mask & 0xFF),
  };
  return google::cloud::internal::UrlsafeBase64EncodeWithPadding(bytes);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
