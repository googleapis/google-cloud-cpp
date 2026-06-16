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

#include "google/cloud/storage/internal/feature_tracker.h"
#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"
#include <memory>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::string EncodeFeatureTrackerBitmask(std::uint32_t mask) {
  if (mask == 0) return {};

  std::vector<std::uint8_t> bytes;
  for (int i = 0; i < 4; ++i) {
    auto b = static_cast<std::uint8_t>((mask >> (24 - i * 8)) & 0xFF);
    if (!bytes.empty() || b != 0) {
      bytes.push_back(b);
    }
  }
  return Base64Encode(bytes);
}

Options SetupFeatureTracker(Options opts) {
  if (opts.has<EnableFeatureReportsOption>() &&
      !opts.get<EnableFeatureReportsOption>()) {
    return std::move(opts);
  }
  if (opts.has<FeatureTrackerOption>()) return std::move(opts);

  std::uint32_t mask = 0;
  // Add checks for configuration-driven options here as they are introduced.
  opts.set<FeatureTrackerOption>(std::make_shared<FeatureTracker>(mask));
  return std::move(opts);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
