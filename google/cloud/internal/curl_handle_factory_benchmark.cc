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

#include "google/cloud/internal/curl_handle_factory.h"
#include <benchmark/benchmark.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class PoolFixture : public ::benchmark::Fixture {
 public:
  PoolFixture() : pool_(128) {}

  CurlHandleFactory& pool() { return pool_; }

 private:
  PooledCurlHandleFactory pool_;
};

bool CreateAndCleanup(CurlHandleFactory& factory) {
  auto h = factory.CreateHandle();
  auto m = factory.CreateMultiHandle();
  auto const success = h && m;
  factory.CleanupMultiHandle(std::move(m), HandleDisposition::kKeep);
  factory.CleanupHandle(std::move(h), HandleDisposition::kKeep);
  return success;
}

// clang-format off
// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//  L1 Data 32 KiB (x48)
//  L1 Instruction 32 KiB (x48)
//  L2 Unified 1024 KiB (x48)
//  L3 Unified 39424 KiB (x2)
// Load Average: 4.17, 4.07, 9.64
// ----------------------------------------------------------------------------------
// Benchmark                                        Time             CPU   Iterations
// ----------------------------------------------------------------------------------
// PoolFixture/Burst/real_time/threads:1          608 ns          608 ns      1226533
// PoolFixture/Burst/real_time/threads:2          776 ns         1550 ns       828674
// PoolFixture/Burst/real_time/threads:4         1164 ns         4234 ns       629056
// PoolFixture/Burst/real_time/threads:8         1291 ns         8838 ns       728752
// PoolFixture/Burst/real_time/threads:16        1095 ns        15615 ns       682480
// PoolFixture/Burst/real_time/threads:32        1258 ns        37116 ns       656256
// PoolFixture/Burst/real_time/threads:64        1821 ns       111173 ns       451008
// PoolFixture/Burst/real_time/threads:128       1946 ns       182346 ns       376192
// PoolFixture/Burst/real_time/threads:256       2005 ns       192227 ns       356608
// clang-format on
BENCHMARK_DEFINE_F(PoolFixture, Burst)(benchmark::State& state) {
  for (auto _ : state) {
    if (!CreateAndCleanup(pool())) {
      state.SkipWithError("error creating handles");
      break;
    }
  }
}
BENCHMARK_REGISTER_F(PoolFixture, Burst)->ThreadRange(1, 1 << 8)->UseRealTime();

// clang-format off
// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//  L1 Data 32 KiB (x48)
//  L1 Instruction 32 KiB (x48)
//  L2 Unified 1024 KiB (x48)
//  L3 Unified 39424 KiB (x2)
// Load Average: 5.36, 4.37, 9.62
// ------------------------------------------------------------------
// Benchmark                        Time             CPU   Iterations
// ------------------------------------------------------------------
// PoolFixture/Linear/1           615 ns          615 ns      1135031
// PoolFixture/Linear/2          1231 ns         1231 ns       567135
// PoolFixture/Linear/4          2469 ns         2469 ns       284093
// PoolFixture/Linear/8          4931 ns         4930 ns       141870
// PoolFixture/Linear/16         9838 ns         9836 ns        70890
// PoolFixture/Linear/32        19701 ns        19696 ns        35525
// PoolFixture/Linear/64        39313 ns        39308 ns        17789
// PoolFixture/Linear/128       78875 ns        78848 ns         8852
// PoolFixture/Linear/256      157491 ns       157468 ns         4436
// PoolFixture/Linear/512      314841 ns       314757 ns         2224
// PoolFixture/Linear/1024     632701 ns       632620 ns         1114
// PoolFixture/Linear_BigO     617.16 N        617.06 N
// PoolFixture/Linear_RMS           0 %             0 %
// clang-format on
BENCHMARK_DEFINE_F(PoolFixture, Linear)(benchmark::State& state) {
  for (auto _ : state) {
    for (auto i = 0; i != state.range(0); ++i) {
      if (!CreateAndCleanup(pool())) {
        state.SkipWithError("error creating handles");
        break;
      }
    }
  }
  state.SetComplexityN(state.range(0));
}
BENCHMARK_REGISTER_F(PoolFixture, Linear)
    ->RangeMultiplier(2)
    ->Ranges({{1, 1 << 10}})
    ->Complexity(benchmark::oN);

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
