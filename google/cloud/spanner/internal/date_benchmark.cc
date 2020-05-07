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

#include "google/cloud/spanner/internal/date.h"
#include <benchmark/benchmark.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

// Run on (6 X 2300 MHz CPU s)
// CPU Caches:
//   L1 Data 32K (x3)
//   L1 Instruction 32K (x3)
//   L2 Unified 256K (x3)
//   L3 Unified 46080K (x1)
// Load Average: 0.24, 0.44, 0.47
// ------------------------------------------------------------
// Benchmark                  Time             CPU   Iterations
// ------------------------------------------------------------
// BM_DateToString          202 ns          200 ns      3443720
// BM_DateFromString        227 ns          226 ns      3099798

void BM_DateToString(benchmark::State& state) {
  Date d(2020, 1, 17);
  for (auto _ : state) {
    benchmark::DoNotOptimize(DateToString(d));
  }
}
BENCHMARK(BM_DateToString);

void BM_DateFromString(benchmark::State& state) {
  std::string s = "2020-01-17";
  for (auto _ : state) {
    benchmark::DoNotOptimize(DateFromString(s));
  }
}
BENCHMARK(BM_DateFromString);

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
