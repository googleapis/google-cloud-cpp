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

#include "google/cloud/options.h"
#include <benchmark/benchmark.h>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x48)
//   L1 Instruction 32 KiB (x48)
//   L2 Unified 1024 KiB (x48)
//   L3 Unified 39424 KiB (x2)
// Load Average: 0.49, 0.35, 0.89
// ----------------------------------------------------------------------
// Benchmark                            Time             CPU   Iterations
// ----------------------------------------------------------------------
// BM_OptionsOneElementDefault       25.6 ns         25.6 ns     27332591
// BM_OptionsOneElementPresent       28.0 ns         28.0 ns     24999101

struct StringOptionDefault {
  using Type = std::string;
};
struct StringOptionPresent {
  using Type = std::string;
};

void BM_OptionsOneElementDefault(benchmark::State& state) {
  auto const opts = Options{}.set<StringOptionPresent>(
      "You will do foolish things, but do them with enthusiasm.");
  for (auto _ : state) {
    auto unused = opts.get<StringOptionDefault>();
    benchmark::DoNotOptimize(unused);
  }
}
BENCHMARK(BM_OptionsOneElementDefault);

void BM_OptionsOneElementPresent(benchmark::State& state) {
  auto const opts = Options{}.set<StringOptionPresent>(
      "You will do foolish things, but do them with enthusiasm.");
  for (auto _ : state) {
    auto unused = opts.get<StringOptionPresent>();
    benchmark::DoNotOptimize(unused);
  }
}
BENCHMARK(BM_OptionsOneElementPresent);

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
