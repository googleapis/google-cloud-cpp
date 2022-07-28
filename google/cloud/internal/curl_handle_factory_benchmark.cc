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
// Load Average: 12.07, 5.96, 3.01
//----------------------------------------------------------------------------------
// Benchmark                                        Time             CPU Iterations
//----------------------------------------------------------------------------------
// PoolFixture/Burst/real_time/threads:1          604 ns          604 ns     1152471
// PoolFixture/Burst/real_time/threads:2          617 ns         1232 ns     1120720
// PoolFixture/Burst/real_time/threads:4         1134 ns         4134 ns      699136
// PoolFixture/Burst/real_time/threads:8         1055 ns         7409 ns      741768
// PoolFixture/Burst/real_time/threads:16        1084 ns        15405 ns      708912
// PoolFixture/Burst/real_time/threads:32        1224 ns        36308 ns      633728
// PoolFixture/Burst/real_time/threads:64        1763 ns       107314 ns      454784
// PoolFixture/Burst/real_time/threads:128       1838 ns       170508 ns      408704
// PoolFixture/Burst/real_time/threads:256       1917 ns       179444 ns      319232
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
// Load Average: 8.49, 4.69, 2.41
//------------------------------------------------------------------
// Benchmark                        Time             CPU   Iterations
//------------------------------------------------------------------
// PoolFixture/Linear/1           565 ns          565 ns      1238384
// PoolFixture/Linear/2          1141 ns         1141 ns       619762
// PoolFixture/Linear/4          2431 ns         2431 ns       287628
// PoolFixture/Linear/8          4858 ns         4857 ns       143748
// PoolFixture/Linear/16         9714 ns         9713 ns        71886
// PoolFixture/Linear/32        19402 ns        19400 ns        35986
// PoolFixture/Linear/64        38784 ns        38782 ns        18028
// PoolFixture/Linear/128       77622 ns        77611 ns         9025
// PoolFixture/Linear/256      155368 ns       155337 ns         4510
// PoolFixture/Linear/512      310703 ns       310682 ns         2256
// PoolFixture/Linear/1024     621968 ns       621804 ns         1125
// PoolFixture/Linear_BigO     607.25 N        607.11 N
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
