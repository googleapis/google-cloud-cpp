// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H

#include "google/cloud/version.h"
#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Retrieve at least @p desired_bits of entropy from `std::random_device`.
 */
std::vector<unsigned int> FetchEntropy(std::size_t desired_bits);

// While std::mt19937_64 is not the best PRNG ever, it is fairly good for
// most purposes.  Please read:
//    http://www.pcg-random.org/
// for a discussion on the topic of PRNGs in general, and the C++ generators
// in particular.
using DefaultPRNG = std::mt19937_64;

/// Create a new PRNG.
inline DefaultPRNG MakeDefaultPRNG() {
  // Fetch a few bits of entropy, which is sufficient for our purposes.
  auto const desired_bits = DefaultPRNG::word_size;
  // Extract the necessary number of entropy bits.
  auto const entropy = FetchEntropy(desired_bits);
  // Finally, put the entropy into the form that the C++11 PRNG classes want.
  // We need a named object because the constructor consumes a reference (and
  // does not consume rvalue references). And `std::seed_seq` is not friendly
  // to "Almost Always Auto" grrr..
  std::seed_seq seq(entropy.begin(), entropy.end());
  return DefaultPRNG(seq);
}

/**
 * Take @p n samples out of @p population, using the @p gen PRNG.
 *
 * Note that sampling is done with repetition, the same element from the
 * population may appear multiple times.
 */
std::string Sample(DefaultPRNG& gen, int n, std::string const& population);

template <
    typename Collection,
    std::enable_if_t<std::is_same<Collection, std::string>::value, int> = 0>
std::string RandomDataToCollection(std::string v) {
  // This is not motivated by a desire to optimize this function (though that is
  // nice). The issue is that I (coryan@) could not figure out how to write a
  // generic version of this function that works with `std::vector<std::byte>`
  // and `std::string`.
  return v;
}

template <
    typename Collection,
    std::enable_if_t<!std::is_same<Collection, std::string>::value, int> = 0>
Collection RandomDataToCollection(std::string v) {
  Collection result(v.size());
  std::transform(v.begin(), v.end(), result.begin(), [](auto c) {
    return static_cast<typename Collection::value_type>(c);
  });
  return result;
}

template <typename Collection>
Collection RandomData(DefaultPRNG& generator, std::size_t size) {
  auto data = Sample(generator, static_cast<int>(size),
                     "abcdefghijklmnopqrstuvwxyz0123456789");
  return RandomDataToCollection<Collection>(std::move(data));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H
