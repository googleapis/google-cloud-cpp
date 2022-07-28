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

// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//  L1 Data 32 KiB (x48)
//  L1 Instruction 32 KiB (x48)
//  L2 Unified 1024 KiB (x48)
//  L3 Unified 39424 KiB (x2)
// Load Average: 15.12, 5.10, 1.98
//----------------------------------------------------------------------------------
// Benchmark                                        Time             CPU   Iterations
//----------------------------------------------------------------------------------
// PoolFixture/Burst/real_time/threads:1          611 ns          611 ns 1138757
// PoolFixture/Burst/real_time/threads:2          753 ns         1503 ns 931064
// PoolFixture/Burst/real_time/threads:4          912 ns         3460 ns 775028
// PoolFixture/Burst/real_time/threads:8          977 ns         6946 ns 719024
// PoolFixture/Burst/real_time/threads:16        1038 ns        14855 ns 645392
// PoolFixture/Burst/real_time/threads:32        1280 ns        38140 ns 570144
// PoolFixture/Burst/real_time/threads:64        1665 ns       101765 ns 510976
// PoolFixture/Burst/real_time/threads:128       1846 ns       172375 ns 388736
// PoolFixture/Burst/real_time/threads:256       1859 ns       180947 ns 377600
BENCHMARK_DEFINE_F(PoolFixture, Burst)(benchmark::State& state) {
  for (auto _ : state) {
    if (!CreateAndCleanup(pool())) {
      state.SkipWithError("error creating handles");
      break;
    }
  }
}
BENCHMARK_REGISTER_F(PoolFixture, Burst)->ThreadRange(1, 1 << 8)->UseRealTime();

// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x48)
//   L1 Instruction 32 KiB (x48)
//   L2 Unified 1024 KiB (x48)
//   L3 Unified 39424 KiB (x2)
// Load Average: 10.79, 7.87, 3.39
//------------------------------------------------------------------
// Benchmark                        Time             CPU   Iterations
//------------------------------------------------------------------
// PoolFixture/Linear/1           628 ns          628 ns      1098609
// PoolFixture/Linear/2          1255 ns         1255 ns       553833
// PoolFixture/Linear/4          2491 ns         2490 ns       278630
// PoolFixture/Linear/8          4967 ns         4967 ns       140583
// PoolFixture/Linear/16         9935 ns         9935 ns        70534
// PoolFixture/Linear/32        19896 ns        19892 ns        35236
// PoolFixture/Linear/64        39706 ns        39700 ns        17636
// PoolFixture/Linear/128       79433 ns        79428 ns         8816
// PoolFixture/Linear/256      159164 ns       159102 ns         4392
// PoolFixture/Linear/512      318709 ns       318687 ns         2202
// PoolFixture/Linear/1024     638671 ns       638514 ns         1098
// PoolFixture/Linear_BigO     623.33 N        623.20 N
// PoolFixture/Linear_RMS           0 %             0 %
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
