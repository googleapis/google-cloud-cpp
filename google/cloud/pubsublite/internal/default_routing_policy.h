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
#include <cassert>
#include <cmath>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class DefaultRoutingPolicy {
 public:
  std::int64_t RouteWithoutKey(std::int64_t num_partitions) {
    return counter_.fetch_add(1, std::memory_order_relaxed) % num_partitions;
  }

  static std::uint64_t Route(std::string const& message_key,
                             std::uint64_t num_partitions) {
    std::vector<std::uint8_t> hash =
        google::cloud::internal::Sha256Hash(message_key);
    assert(hash.size() == 256 / 8);
    std::uint64_t big_endian_value = 0;
    std::unordered_map<std::uint8_t, std::uint64_t> mods;
    for (unsigned int i = 0; i < hash.size(); ++i) {
      for (unsigned int j = 0; j < 8; ++j) {
        uint8_t last_bit = (hash[i] & static_cast<std::uint8_t>(1));
        if (last_bit == 1) {
          big_endian_value += DivideAndConquer((hash.size() - i - 1) * 8 + j,
                                               num_partitions, mods) %
                              num_partitions;
        }
        hash[i] >>= 1;
      }
    }
    return big_endian_value % num_partitions;
  }

 private:
  static std::uint64_t DivideAndConquer(
      std::uint8_t exp, std::uint64_t num_partitions,
      std::unordered_map<std::uint8_t, std::uint64_t>& mods) {
    if (exp < 64) {
      return static_cast<std::uint64_t>(std::pow<std::uint64_t>(2, exp)) %
             num_partitions;
    }
    if (mods.find(exp) == mods.end()) {
      mods[exp] = (2 * DivideAndConquer(exp - 1, num_partitions, mods)) %
                  num_partitions;
    }
    return mods[exp];
  }
  std::atomic<std::int64_t> counter_;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_DEFAULT_ROUTING_POLICY_H
