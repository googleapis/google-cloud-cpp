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

#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/version.h"
#include <atomic>
#include <cstddef>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class DefaultRoutingPolicy {
 public:
  std::uint64_t RouteWithoutKey(std::int64_t num_partitions) {
    return counter_.fetch_add(1, std::memory_order_relaxed) % num_partitions;
  }

  static std::uint64_t Route(std::string const& message_key,
                             std::uint64_t num_partitions) {
    return GetMod(google::cloud::internal::Sha256Hash(message_key),
                  num_partitions);
  }

 private:
  // Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
  static std::uint64_t ModPow(std::uint64_t val, std::uint64_t pow,
                              std::uint64_t mod) {
    std::uint64_t result = 1;
    for (std::uint64_t i = 0; i < pow; ++i) {
      result *= (val % mod);
      result %= mod;
    }
    return result;
  }

  // Uses the identity that `(a*b) % m == ((a % m) * (b % m)) % m`
  // Uses the identity that `(a+b) % m == ((a % m) + (b % m)) % m`
  static std::uint64_t GetMod(std::array<uint8_t, 32> big_endian,
                              std::uint64_t mod) {
    std::uint64_t result = 0;
    for (std::uint64_t i = 0; i < big_endian.size(); ++i) {
      std::uint64_t val_mod = big_endian[i] % mod;

      std::uint64_t pow = big_endian.size() - (i + 1);
      std::uint64_t pow_mod = ModPow(
          // 2^8
          static_cast<std::uint64_t>(2 * 2 * 2 * 2 * 2 * 2 * 2 * 2), pow, mod);

      result += (val_mod * pow_mod) % mod;
      result %= mod;
    }
    return result;
  }

  std::atomic<std::int64_t> counter_{0};

  friend class TestGetMod;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_DEFAULT_ROUTING_POLICY_H
