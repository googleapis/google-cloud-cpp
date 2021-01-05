// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/bytes.h"
#include <benchmark/benchmark.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

// Run on (6 X 2300 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x3)
//   L1 Instruction 32K (x3)
//   L2 Unified 256K (x3)
//   L3 Unified 46080K (x1)
// Load Average: 0.22, 0.24, 0.26
// ----------------------------------------------------------------
// Benchmark            Time       CPU   Iterations UserCounters...
// ----------------------------------------------------------------
// BM_BytesCtor      8386 ns   8318 ns        82963 bytes_per_second=167.843M/s
// BM_BytesGet       6094 ns   6057 ns       115399 bytes_per_second=307.34M/s

std::string const kText = R"""(
    Four score and seven years ago our fathers brought forth on this
    continent, a new nation, conceived in Liberty, and dedicated to
    the proposition that all men are created equal.

    Now we are engaged in a great civil war, testing whether that
    nation, or any nation so conceived and so dedicated, can long
    endure. We are met on a great battle-field of that war. We have
    come to dedicate a portion of that field, as a final resting
    place for those who here gave their lives that that nation might
    live. It is altogether fitting and proper that we should do this.

    But, in a larger sense, we can not dedicate - we can not
    consecrate - we can not hallow-this ground. The brave men, living
    and dead, who struggled here, have consecrated it, far above our
    poor power to add or detract. The world will little note, nor long
    remember what we say here, but it can never forget what they did
    here. It is for us the living, rather, to be dedicated here to the
    unfinished work which they who fought here have thus far so nobly
    advanced. It is rather for us to be here dedicated to the great
    task remaining before us - that from these honored dead we take
    increased devotion to that cause for which they gave the last full
    measure of devotion - that we here highly resolve that these dead
    shall not have died in vain - that this nation, under God, shall
    have a new birth of freedom - and that government of the people,
    by the people, for the people, shall not perish from the earth.
    )""";

void BM_BytesCtor(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(Bytes(kText));
  }
  state.SetBytesProcessed(state.iterations() * kText.size());
}
BENCHMARK(BM_BytesCtor);

void BM_BytesGet(benchmark::State& state) {
  Bytes b(kText);
  for (auto _ : state) {
    benchmark::DoNotOptimize(b.get<std::string>());
  }
  state.SetBytesProcessed(state.iterations() *
                          spanner_internal::BytesToBase64(b).size());
}
BENCHMARK(BM_BytesGet);

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
