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

#include "bigtable/benchmarks/random.h"
#include "bigtable/benchmarks/constants.h"

namespace bigtable {
namespace benchmarks {
std::string Sample(DefaultPRNG& gen, int n, std::string const& population) {
  std::uniform_int_distribution<std::size_t> rd(0, population.size() - 1);

  std::string result(std::size_t(n), '0');
  std::generate(result.begin(), result.end(),
                [&rd, &gen, &population]() { return population[rd(gen)]; });
  return result;
}

bigtable::Mutation MakeRandomMutation(DefaultPRNG& gen, int f) {
  std::string field = "field" + std::to_string(f);
  return bigtable::SetCell(kColumnFamily, std::move(field),
                           std::chrono::milliseconds(0), MakeRandomValue(gen));
}

std::string MakeRandomValue(DefaultPRNG& generator) {
  static std::string const letters(
      "ABCDEFGHIJLKMNOPQRSTUVWXYZabcdefghijlkmnopqrstuvwxyz0123456789-/_");
  return Sample(generator, kFieldSize, letters);
}

}  // namespace benchmarks
}  // namespace bigtable
