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
// Load Average: 7.87, 6.41, 5.61
//----------------------------------------------------------------------------------
// Benchmark                                        Time             CPU Iterations
//----------------------------------------------------------------------------------
// PoolFixture/Burst/real_time/threads:1          626 ns          626 ns 1115290
// PoolFixture/Burst/real_time/threads:2          838 ns         1631 ns 887016
// PoolFixture/Burst/real_time/threads:4          976 ns         3640 ns 570456
// PoolFixture/Burst/real_time/threads:8         1172 ns         8115 ns 646784
// PoolFixture/Burst/real_time/threads:16        1398 ns        19700 ns 594416
// PoolFixture/Burst/real_time/threads:32        1850 ns        54669 ns 592704
// PoolFixture/Burst/real_time/threads:64        1877 ns       114773 ns 400960
// PoolFixture/Burst/real_time/threads:128       2100 ns       198027 ns 335360
// PoolFixture/Burst/real_time/threads:256       2170 ns       210219 ns 256000
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
// Load Average: 2.91, 2.31, 3.68
//------------------------------------------------------------------
// Benchmark                        Time             CPU   Iterations
//------------------------------------------------------------------
// PoolFixture/Linear/1           602 ns          602 ns      1181239
// PoolFixture/Linear/2          1269 ns         1269 ns       551564
// PoolFixture/Linear/4          2535 ns         2535 ns       275365
// PoolFixture/Linear/8          5071 ns         5069 ns       137769
// PoolFixture/Linear/16        10146 ns        10143 ns        69062
// PoolFixture/Linear/32        20283 ns        20280 ns        34552
// PoolFixture/Linear/64        40571 ns        40566 ns        17277
// PoolFixture/Linear/128       81125 ns        81114 ns         8640
// PoolFixture/Linear/256      162330 ns       162304 ns         4316
// PoolFixture/Linear/512      324911 ns       324844 ns         2159
// PoolFixture/Linear/1024     650354 ns       650268 ns         1080
// PoolFixture/Linear_BigO     634.95 N        634.85 N
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
