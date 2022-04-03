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

#include "google/cloud/pubsublite/internal/default_routing_policy.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/version.h"
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

// will always be 32 as specified in sha256_hash.h
auto constexpr kNumBytesSha256 = 32;

// Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
RoutingPolicy::Partition GetBinaryMultOffset(std::uint32_t num_bytes,
                                             RoutingPolicy::Partition mod) {
  std::uint32_t result = 1;
  for (std::uint32_t i = 0; i < num_bytes; ++i) {
    // 2^8
    result *= (static_cast<std::uint32_t>(1 << 8) % mod);
    result %= mod;
  }
  return result;
}

// Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
// Uses the identity that `(a+b) % m == ((a % m) + (b % m)) % m`
RoutingPolicy::Partition GetMod(std::array<uint8_t, kNumBytesSha256> big_endian,
                                RoutingPolicy::Partition mod) {
  std::uint32_t result = 0;
  for (std::uint32_t i = 0; i < kNumBytesSha256; ++i) {
    std::uint32_t val_mod = big_endian[i] % mod;

    std::uint32_t num_bytes = kNumBytesSha256 - (i + 1);
    std::uint32_t mult_offset_mod = GetBinaryMultOffset(num_bytes, mod);

    result += (val_mod * mult_offset_mod) % mod;
    result %= mod;
  }
  return result;
}

RoutingPolicy::Partition DefaultRoutingPolicy::Route(Partition num_partitions) {
  // atomic operation
  return counter_++ % num_partitions;
}

RoutingPolicy::Partition DefaultRoutingPolicy::Route(
    std::string const& message_key, Partition num_partitions) {
  return GetMod(google::cloud::internal::Sha256Hash(message_key),
                num_partitions);
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
