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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TRACKED_FEATURES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TRACKED_FEATURES_H

#include "google/cloud/storage/version.h"
#include "google/cloud/internal/rest_context.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The header (REST) and metadata key (gRPC) carrying the client feature mask.
 *
 * The value is a 4-byte big-endian `std::uint32_t` bitmask, encoded as
 * URL-safe base64 (RFC 4648 section 5, `-` and `_` alphabet) with `=` padding.
 * The service decodes only the URL-safe alphabet; a value in the standard
 * alphabet (`+` and `/`) is rejected. The header is omitted when no tracked
 * feature is in use.
 */
inline constexpr char kFeaturesHeader[] = "x-goog-storage-cpp-features";

/**
 * Client-side features reported to the service.
 *
 * Each enumerator is the bit position (not the bit value) of the feature in
 * the mask. Bit positions are specific to this library: the service keeps a
 * separate bit-to-feature table per client header (that is, per library),
 * populated per release by an internal tracker that parses this file.
 * Positions therefore need not match other libraries (e.g. the Go SDK uses
 * bit 1 for a different feature). Never renumber or reuse a bit once released,
 * and keep each enumerator on one line, in the form `kName = N,`.
 */
enum class TrackedFeature : std::uint32_t {
  kMultistreamInMrd = 0,
  kReadHedging = 1,
};

/// Every tracked feature. Add new enumerators here too.
inline constexpr std::array<TrackedFeature, 2> kAllTrackedFeatures{
    TrackedFeature::kMultistreamInMrd, TrackedFeature::kReadHedging};

/**
 * Returns true if every feature is a bit position below 32 and no two
 * features share a position.
 *
 * The mask is 32 bits wide, so a position of 32 or more would be silently
 * dropped, and two features on the same bit would be indistinguishable.
 */
constexpr bool TrackedFeaturesAreValid() {
  std::uint32_t all_bits = 0;
  for (TrackedFeature feature : kAllTrackedFeatures) {
    if (static_cast<std::uint32_t>(feature) >= 32) return false;
    all_bits |= std::uint32_t{1} << static_cast<std::uint32_t>(feature);
  }
  // With distinct positions the OR has exactly one bit per feature.
  std::size_t set_bits = 0;
  for (; all_bits != 0; all_bits &= all_bits - 1) ++set_bits;
  return set_bits == kAllTrackedFeatures.size();
}

static_assert(TrackedFeaturesAreValid(),
              "TrackedFeature values must be distinct bit positions below 32");

/// Returns the bit value (`1 << position`) for @p feature.
constexpr std::uint32_t FeatureBit(TrackedFeature feature) {
  return std::uint32_t{1} << static_cast<std::uint32_t>(feature);
}

/**
 * Encodes @p mask as the value of `kFeaturesHeader`.
 *
 * Returns an empty string when @p mask is zero. Callers that just want the
 * header set (or not) should prefer `AddFeaturesHeader()`, which handles that
 * case.
 */
std::string EncodeFeatureMask(std::uint32_t mask);

/**
 * Sets `kFeaturesHeader` on @p context to the encoding of @p mask.
 *
 * Does nothing when @p mask is zero: the header is omitted, not sent empty.
 * The gRPC equivalent lives in `internal/grpc/configure_client_context.h`,
 * as this library does not depend on gRPC.
 */
void AddFeaturesHeader(rest_internal::RestContext& context, std::uint32_t mask);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_TRACKED_FEATURES_H
