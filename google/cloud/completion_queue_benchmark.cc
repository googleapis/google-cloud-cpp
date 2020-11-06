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

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/default_completion_queue_impl.h"
#include <benchmark/benchmark.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

// Run on (96 X 2000.15 MHz CPU s)
// CPU Caches:
//  L1 Data 32K (x48)
//  L1 Instruction 32K (x48)
//  L2 Unified 1024K (x48)
//  L3 Unified 39424K (x2)
// Load Average: 2.90, 9.21, 73.15
//-----------------------------------------------------------------------------
// Benchmark                                   Time             CPU   Iterations
//-----------------------------------------------------------------------------
// BM_Baseline/16/512                      15345 ns        15344 ns        44845
// BM_Baseline/16/1024                     30807 ns        30802 ns        22416
// BM_Baseline/16/2048                     62403 ns        62390 ns        11096
// BM_Baseline_BigO                        30.37 N         30.37 N
// BM_Baseline_RMS                             1 %             1 %
// BM_CompletionQueueRunAsync/16/512      916720 ns       189304 ns         3450
// BM_CompletionQueueRunAsync/16/1024     990193 ns       243485 ns         2668
// BM_CompletionQueueRunAsync/16/2048    1976575 ns       420878 ns         1713
// BM_CompletionQueueRunAsync_BigO       1004.78 N        219.47 N
// BM_CompletionQueueRunAsync_RMS             18 %            17 %

auto constexpr kMinThreads = 16;
auto constexpr kMaxThreads = 16;
auto constexpr kMinExecutions = 1 << 9;
auto constexpr kMaxExecutions = 1 << 11;

class Wait {
 public:
  explicit Wait(std::int64_t count) : count_(count) {}

  void BlockUntilDone() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return count_ == 0; });
  }

  void OneDone() {
    std::unique_lock<std::mutex> lk(mu_);
    if (--count_ == 0) cv_.notify_one();
    lk.unlock();
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::int64_t count_;
};

void BM_Baseline(benchmark::State& state) {
  auto runner = [&](std::int64_t n) {
    Wait wait(n);
    std::deque<std::function<void()>> queue;
    for (std::int64_t i = 0; i != n; ++i) {
      queue.emplace_back([&wait] { wait.OneDone(); });
    }
    while (!queue.empty()) {
      auto f = std::move(queue.front());
      queue.pop_front();
      f();
    }
    wait.BlockUntilDone();
    return 0;
  };

  for (auto _ : state) {
    benchmark::DoNotOptimize(runner(state.range(1)));
  }
  state.SetComplexityN(state.range(1));
}
BENCHMARK(BM_Baseline)
    ->RangeMultiplier(2)
    ->Ranges({{kMinThreads, kMaxThreads}, {kMinExecutions, kMaxExecutions}})
    ->Complexity(benchmark::oN);

void BM_CompletionQueueRunAsync(benchmark::State& state) {
  CompletionQueue cq;
  std::vector<std::thread> tasks(static_cast<std::size_t>(state.range(0)));
  std::generate(tasks.begin(), tasks.end(), [&cq] {
    return std::thread{[](CompletionQueue cq) { cq.Run(); }, cq};
  });

  auto runner = [&](std::int64_t n) {
    Wait wait(n);
    for (std::int64_t i = 0; i != n; ++i) {
      cq.RunAsync([&wait] { wait.OneDone(); });
    }
    wait.BlockUntilDone();
    return 0;
  };

  for (auto _ : state) {
    benchmark::DoNotOptimize(runner(state.range(1)));
  }
  state.SetComplexityN(state.range(1));
  cq.Shutdown();
  for (auto& t : tasks) t.join();
}
BENCHMARK(BM_CompletionQueueRunAsync)
    ->RangeMultiplier(2)
    ->Ranges({{kMinThreads, kMaxThreads}, {kMinExecutions, kMaxExecutions}})
    ->Complexity(benchmark::oN);

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
