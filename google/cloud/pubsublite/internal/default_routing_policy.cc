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
#include "google/cloud/version.h"
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

// Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
std::uint64_t ModPow(std::uint64_t val, std::uint64_t pow, std::uint32_t mod) {
  std::uint64_t result = 1;
  for (std::uint32_t i = 0; i < pow; ++i) {
    result *= (val % mod);
    result %= mod;
  }
  return result;
}

// Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
// Uses the identity that `(a+b) % m == ((a % m) + (b % m)) % m`
std::uint64_t GetMod(std::array<uint8_t, 32> big_endian, std::uint32_t mod) {
  std::uint64_t result = 0;
  for (std::uint64_t i = 0; i < big_endian.size(); ++i) {
    std::uint64_t val_mod = big_endian[i] % mod;

    std::uint64_t pow = big_endian.size() - (i + 1);
    std::uint64_t pow_mod = ModPow(
        // 2^8
        static_cast<std::uint64_t>(1 << 8), pow, mod);

    result += (val_mod * pow_mod) % mod;
    result %= mod;
  }
  return result;
}

std::uint64_t DefaultRoutingPolicy::Route(std::uint32_t num_partitions) {
  // atomic operation
  return counter_++ % num_partitions;
}

std::uint64_t DefaultRoutingPolicy::Route(std::string const& message_key,
                                          std::uint32_t num_partitions) {
  return GetMod(google::cloud::internal::Sha256Hash(message_key),
                num_partitions);
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
