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

#include "google/cloud/options.h"
#include <benchmark/benchmark.h>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Run on (128 X 2250 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x64)
//   L1 Instruction 32 KiB (x64)
//   L2 Unified 512 KiB (x64)
//   L3 Unified 16384 KiB (x16)
// Load Average: 1.26, 1.39, 2.94
// --------------------------------------------------------------------------
// Benchmark                                Time             CPU   Iterations
// --------------------------------------------------------------------------
// BM_OptionsOneElementDefault           18.4 ns         18.4 ns     38006008
// BM_OptionsOneElementPresent           45.8 ns         45.8 ns     15266572
// BM_SimulateRpc                       10721 ns        10721 ns        64131
// BM_SimulateStreamingRpc             891912 ns       891888 ns          774
// BM_SimulateStreamingRpcWithSave      12547 ns        12547 ns        54804

struct StringOptionDefault {
  using Type = std::string;
};
struct StringOptionPresent {
  using Type = std::string;
};

void BM_OptionsOneElementDefault(benchmark::State& state) {
  auto const opts = Options{}.set<StringOptionPresent>(
      "You will do foolish things, but do them with enthusiasm.");
  for (auto _ : state) {
    auto unused = opts.get<StringOptionDefault>();
    benchmark::DoNotOptimize(std::move(unused));
  }
}
BENCHMARK(BM_OptionsOneElementDefault);

void BM_OptionsOneElementPresent(benchmark::State& state) {
  auto const opts = Options{}.set<StringOptionPresent>(
      "You will do foolish things, but do them with enthusiasm.");
  for (auto _ : state) {
    auto unused = opts.get<StringOptionPresent>();
    benchmark::DoNotOptimize(std::move(unused));
  }
}
BENCHMARK(BM_OptionsOneElementPresent);

template <int I>
struct TestOption {
  using Type = int;
};

template <int I>
struct PopulateOptions;
template <>
struct PopulateOptions<0> {
  Options operator()() const { return Options{}.set<TestOption<0>>(0); }
};
template <int I>
struct PopulateOptions {
  Options operator()() const {
    return PopulateOptions<I - 1>{}().template set<TestOption<I>>(I);
  }
};

template <int I>
struct ReadAllOptions;
template <>
struct ReadAllOptions<0> {
  int operator()(Options const& o) const { return o.get<TestOption<0>>(); }
};
template <int I>
struct ReadAllOptions {
  int operator()(Options const& o) const {
    return ReadAllOptions<I - 1>{}(o) + o.get<TestOption<I>>();
  }
};

auto constexpr kOptionCount = 64;
auto constexpr kMessageCount = 100;

std::string SimulateRpc(Options overrides, Options const& client) {
  internal::OptionsSpan span(
      internal::MergeOptions(std::move(overrides), client));
  auto const& current = internal::CurrentOptions();
  return std::to_string(ReadAllOptions<kOptionCount>{}(current));
}

std::string SimulateStreamingRPCWithSave(Options overrides,
                                         Options const& client) {
  internal::OptionsSpan span(
      internal::MergeOptions(std::move(overrides), client));
  auto current = internal::SaveCurrentOptions();
  for (int i = 0; i != kMessageCount; ++i) {
    internal::OptionsSpan tmp(current);
  }
  return std::to_string(ReadAllOptions<kOptionCount>{}(*current));
}

std::string SimulateStreamingRPC(Options overrides, Options const& client) {
  internal::OptionsSpan span(
      internal::MergeOptions(std::move(overrides), client));
  auto const& current = internal::CurrentOptions();
  for (int i = 0; i != kMessageCount; ++i) {
    internal::OptionsSpan tmp(current);
  }
  return std::to_string(ReadAllOptions<kOptionCount>{}(current));
}

void BM_SimulateRpc(benchmark::State& state) {
  auto const client = PopulateOptions<kOptionCount>{}();
  for (auto _ : state) {
    benchmark::DoNotOptimize(SimulateRpc(Options{}, client));
  }
}
BENCHMARK(BM_SimulateRpc);

void BM_SimulateStreamingRpc(benchmark::State& state) {
  auto const client = PopulateOptions<kOptionCount>{}();
  for (auto _ : state) {
    benchmark::DoNotOptimize(SimulateStreamingRPC(Options{}, client));
  }
}
BENCHMARK(BM_SimulateStreamingRpc);

void BM_SimulateStreamingRpcWithSave(benchmark::State& state) {
  auto const client = PopulateOptions<kOptionCount>{}();
  for (auto _ : state) {
    benchmark::DoNotOptimize(SimulateStreamingRPCWithSave(Options{}, client));
  }
}
BENCHMARK(BM_SimulateStreamingRpcWithSave);

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
