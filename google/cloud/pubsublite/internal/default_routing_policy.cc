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
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// will always be 32 as specified in sha256_hash.h
std::uint32_t constexpr kNumBytesSha256 = 32;

// Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
std::uint64_t ModPow(std::uint64_t val, std::uint32_t pow, Partition mod) {
  std::uint64_t result = 1;
  for (std::uint32_t i = 0; i != pow; ++i) {
    result *= (val % mod);
    result %= mod;
  }
  return result;
}

// Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
// Uses the identity that `(a+b) % m == ((a % m) + (b % m)) % m`
Partition GetMod(std::array<uint8_t, kNumBytesSha256> big_endian,
                 Partition mod) {
  std::uint64_t result = 0;
  for (std::uint32_t i = 0; i != kNumBytesSha256; ++i) {
    std::uint32_t val_mod = big_endian[i] % mod;

    std::uint32_t pow = kNumBytesSha256 - (i + 1);
    std::uint64_t pow_mod = ModPow(256, pow, mod);

    result += (val_mod * pow_mod) % mod;
    result %= mod;
  }
  // within bounds because `mod`ed by a std::uint32_t value
  return static_cast<std::uint32_t>(result);
}

Partition DefaultRoutingPolicy::Route(Partition num_partitions) {
  // atomic operation
  return static_cast<std::uint32_t>(counter_++ % num_partitions);
}

Partition DefaultRoutingPolicy::Route(std::string const& message_key,
                                      Partition num_partitions) {
  return GetMod(google::cloud::internal::Sha256Hash(message_key),
                num_partitions);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google
