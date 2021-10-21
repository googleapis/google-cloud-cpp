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

#include "google/cloud/spanner/numeric.h"
#include <benchmark/benchmark.h>
#include <cstdint>
#include <limits>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

// Run on (96 X 2000.17 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x48)
//   L1 Instruction 32K (x48)
//   L2 Unified 1024K (x48)
//   L3 Unified 39424K (x2)
// Load Average: 0.22, 1.93, 3.67
// ------------------------------------------------------------------------
// Benchmark                              Time             CPU   Iterations
// ------------------------------------------------------------------------
// BM_NumericFromStringCanonical       76.0 ns         76.0 ns      8999376
// BM_NumericFromString                 369 ns          369 ns      1925749
// BM_NumericFromDouble                2072 ns         2072 ns       340812
// BM_NumericFromUnsigned               168 ns          168 ns      4151959
// BM_NumericFromInteger                165 ns          165 ns      4238271
// BM_NumericToString                 0.360 ns        0.360 ns   1000000000
// BM_NumericToDouble                   115 ns          115 ns      6120955
// BM_NumericToUnsigned                83.8 ns         83.8 ns      8466831
// BM_NumericToInteger                 86.3 ns         86.3 ns      8092890

void BM_NumericFromStringCanonical(benchmark::State& state) {
  std::string s = "99999999999999999999999999999.999999999";
  for (auto _ : state) {
    benchmark::DoNotOptimize(MakeNumeric(s));
  }
}
BENCHMARK(BM_NumericFromStringCanonical);

void BM_NumericFromString(benchmark::State& state) {
  std::string s = "+9999999999999999999999999999.9999999999e1";
  for (auto _ : state) {
    benchmark::DoNotOptimize(MakeNumeric(s));
  }
}
BENCHMARK(BM_NumericFromString);

void BM_NumericFromDouble(benchmark::State& state) {
  double d = 9.999999999999999e+28;
  for (auto _ : state) {
    benchmark::DoNotOptimize(MakeNumeric(d));
  }
}
BENCHMARK(BM_NumericFromDouble);

void BM_NumericFromUnsigned(benchmark::State& state) {
  auto u = std::numeric_limits<std::uint64_t>::max();
  for (auto _ : state) {
    benchmark::DoNotOptimize(MakeNumeric(u));
  }
}
BENCHMARK(BM_NumericFromUnsigned);

void BM_NumericFromInteger(benchmark::State& state) {
  auto i = std::numeric_limits<std::int64_t>::min();
  for (auto _ : state) {
    benchmark::DoNotOptimize(MakeNumeric(i));
  }
}
BENCHMARK(BM_NumericFromInteger);

void BM_NumericToString(benchmark::State& state) {
  std::string s = "99999999999999999999999999999.999999999";
  Numeric n = MakeNumeric(s).value();
  for (auto _ : state) {
    benchmark::DoNotOptimize(n.ToString());
  }
}
BENCHMARK(BM_NumericToString);

void BM_NumericToDouble(benchmark::State& state) {
  double d = 9.999999999999999e+28;
  Numeric n = MakeNumeric(d).value();
  for (auto _ : state) {
    benchmark::DoNotOptimize(ToDouble(n));
  }
}
BENCHMARK(BM_NumericToDouble);

void BM_NumericToUnsigned(benchmark::State& state) {
  auto u = std::numeric_limits<std::uint64_t>::max();
  Numeric n = MakeNumeric(u).value();
  for (auto _ : state) {
    benchmark::DoNotOptimize(ToInteger<std::uint64_t>(n));
  }
}
BENCHMARK(BM_NumericToUnsigned);

void BM_NumericToInteger(benchmark::State& state) {
  auto i = std::numeric_limits<std::int64_t>::min();
  Numeric n = MakeNumeric(i).value();
  for (auto _ : state) {
    benchmark::DoNotOptimize(ToInteger<std::int64_t>(n));
  }
}
BENCHMARK(BM_NumericToInteger);

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
