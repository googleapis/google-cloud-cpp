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

#include "google/cloud/spanner/row.h"
#include <benchmark/benchmark.h>

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
// Load Average: 2.87, 2.31, 2.15
// -----------------------------------------------------------------------
// Benchmark                             Time             CPU   Iterations
// -----------------------------------------------------------------------
// BM_RowGetByPosition                 134 ns          133 ns      5258635
// BM_RowGetByColumnName               195 ns          194 ns      3590333

void BM_RowGetByPosition(benchmark::State& state) {
  Row row = MakeTestRow(1, "blah", true);
  for (auto _ : state) {
    benchmark::DoNotOptimize(row.get(0));
    benchmark::DoNotOptimize(row.get(1));
    benchmark::DoNotOptimize(row.get(2));
  }
}
BENCHMARK(BM_RowGetByPosition);

void BM_RowGetByColumnName(benchmark::State& state) {
  Row row = MakeTestRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });
  for (auto _ : state) {
    benchmark::DoNotOptimize(row.get("a"));
    benchmark::DoNotOptimize(row.get("b"));
    benchmark::DoNotOptimize(row.get("c"));
  }
}
BENCHMARK(BM_RowGetByColumnName);

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
