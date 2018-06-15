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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H_

#include "google/cloud/version.h"
#include <algorithm>
#include <random>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
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
  // We use the default C++ random device to generate entropy.  The quality of
  // this source of entropy is implementation-defined. The version in
  // Linux with libstdc++ (the most common library on Linux) it is based on
  // either `/dev/urandom`, or (if available) the RDRAND CPU instruction.
  //     http://en.cppreference.com/w/cpp/numeric/random/random_device/random_device
  // On Linux with libc++ it is also based on `/dev/urandom`, but it is not
  // known if it uses the RDRND CPU instruction (though `/dev/urandom` does).
  // On Windows the documentation says that the numbers are not deterministic,
  // and cryptographically secure, but no further details are available:
  //     https://docs.microsoft.com/en-us/cpp/standard-library/random-device-class
  // One would want to simply pass this object to the constructor for the
  // generator, but the C++11 approach is annoying, see this critique for
  // the details:
  //     http://www.pcg-random.org/posts/simple-portable-cpp-seed-entropy.html
  // Instead we will do a lot of work below.
  std::random_device rd;

  // We need to get enough entropy to fully initialize the PRNG, that depends
  // on the size of its state.  The size in bits is `Generator::state_size`.
  // We convert that to the number of `unsigned int` values; as this is what
  // std::random_device returns.
  auto const S =
      Generator::state_size *
      (Generator::word_size / std::numeric_limits<unsigned int>::digits);

  // Extract the necessary number of entropy bits.
  std::vector<unsigned int> entropy(S);
  std::generate(entropy.begin(), entropy.end(), [&rd]() { return rd(); });

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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RANDOM_H_
