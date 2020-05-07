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

#include "google/cloud/spanner/internal/time_format.h"
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
// Load Average: 0.22, 0.26, 0.77
// ---------------------------------------------------------------
// Benchmark                     Time             CPU   Iterations
// ---------------------------------------------------------------
// BM_FormatTime              55.2 ns         54.7 ns     12779010
// BM_FormatTimeWithFmt       1047 ns         1042 ns       668990
// BM_ParseTime                129 ns          128 ns      5474604
// BM_ParseTimeWithFmt        1323 ns         1314 ns       489764

void BM_FormatTime(benchmark::State& state) {
  std::tm tm;
  tm.tm_year = 2020 - 1900;
  tm.tm_mon = 1 - 1;
  tm.tm_mday = 17;
  tm.tm_hour = 18;
  tm.tm_min = 54;
  tm.tm_sec = 12;
  for (auto _ : state) {
    benchmark::DoNotOptimize(FormatTime(tm));
  }
}
BENCHMARK(BM_FormatTime);

void BM_FormatTimeWithFmt(benchmark::State& state) {
  std::tm tm;
  tm.tm_year = 2020 - 1900;
  tm.tm_mon = 1 - 1;
  tm.tm_mday = 17;
  tm.tm_hour = 18;
  tm.tm_min = 54;
  tm.tm_sec = 12;
  for (auto _ : state) {
    benchmark::DoNotOptimize(FormatTime("%Y-%m-%dT%H:%M:%S", tm));
  }
}
BENCHMARK(BM_FormatTimeWithFmt);

void BM_ParseTime(benchmark::State& state) {
  std::tm tm;
  std::string s = "2020-01-17T18:54:12";
  for (auto _ : state) {
    benchmark::DoNotOptimize(ParseTime(s, &tm));
  }
}
BENCHMARK(BM_ParseTime);

void BM_ParseTimeWithFmt(benchmark::State& state) {
  std::tm tm;
  std::string s = "2020-01-17T18:54:12";
  for (auto _ : state) {
    benchmark::DoNotOptimize(ParseTime("%Y-%m-%dT%H:%M:%S", s, &tm));
  }
}
BENCHMARK(BM_ParseTimeWithFmt);

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
