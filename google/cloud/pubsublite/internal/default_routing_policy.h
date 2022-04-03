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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_DEFAULT_ROUTING_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_DEFAULT_ROUTING_POLICY_H

#include "google/cloud/pubsublite/internal/routing_policy.h"
#include "google/cloud/version.h"
#include <atomic>
#include <cstddef>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

// Calculates the val^pow % mod while accounting for overflow.
// Needed because after calculating `big_endian[i]` % `mod` in `GetMod`, we need
// to account for its position in the array by multiplying it by an offset.
std::uint64_t ModPow(std::uint64_t val, std::uint64_t pow, std::uint32_t mod);

// returns <integer value of `big_endian`> % `mod` while accounting for overflow
std::uint64_t GetMod(std::array<uint8_t, 32> big_endian, std::uint32_t mod);

/**
 * Implements the same routing policy as all the other Pub/Sub Lite client
 * libraries.
 *
 * All the client libraries provided by Google use the same algorithm to route
 * messages.
 *
 * @note The algorithm for routing with a message key is <big-endian integer
 * representation of SHA256(message key)> % <number of partitions>. It uses
 * SHA-256 as it is available in most programming languages, enabling consistent
 * hashing across languages.
 */
class DefaultRoutingPolicy : public RoutingPolicy {
 public:
  Partition Route(Partition num_partitions) override;
  Partition Route(std::string const& message_key,
                  Partition num_partitions) override;

 private:
  std::atomic<std::uint32_t> counter_{0};
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_DEFAULT_ROUTING_POLICY_H
