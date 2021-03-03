// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H

#include "google/cloud/version.h"
#include <random>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
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

/**
 * Initialize any of the C++11 PRNGs in `<random>` using std::random_device.
 */
template <typename Generator>
Generator MakePRNG() {
  // We need to get enough entropy to fully initialize the PRNG, that depends
  // on the size of its state.  The size in bits is `Generator::state_size`.
  // We convert that to the number of `unsigned int` values; as this is what
  // std::random_device returns.
  auto const desired_bits = Generator::state_size * Generator::word_size;

  // Extract the necessary number of entropy bits.
  auto const entropy = FetchEntropy(desired_bits);

  // Finally, put the entropy into the form that the C++11 PRNG classes want.
  std::seed_seq seq(entropy.begin(), entropy.end());
  return Generator(seq);
}

/// Create a new PRNG.
inline DefaultPRNG MakeDefaultPRNG() { return MakePRNG<DefaultPRNG>(); }

/**
 * Take @p n samples out of @p population, using the @p gen PRNG.
 *
 * Note that sampling is done with repetition, the same element from the
 * population may appear multiple times.
 */
std::string Sample(DefaultPRNG& gen, int n, std::string const& population);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H
