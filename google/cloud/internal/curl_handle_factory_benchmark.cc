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
  factory.CleanupMultiHandle(std::move(m), CurlHandleFactory::kKeep);
  factory.CleanupHandle(std::move(h), CurlHandleFactory::kKeep);
  return success;
}

BENCHMARK_DEFINE_F(PoolFixture, Burst)(benchmark::State& state) {
  for (auto _ : state) {
    if (!CreateAndCleanup(pool())) {
      state.SkipWithError("error creating handles");
      break;
    }
  }
}
BENCHMARK_REGISTER_F(PoolFixture, Burst)->ThreadRange(1, 1 << 8)->UseRealTime();

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
