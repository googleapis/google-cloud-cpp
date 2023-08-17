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

// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x48)
//   L1 Instruction 32 KiB (x48)
//   L2 Unified 1024 KiB (x48)
//   L3 Unified 39424 KiB (x2)
// Load Average: 0.49, 0.35, 0.89
// ----------------------------------------------------------------------
// Benchmark                            Time             CPU   Iterations
// ----------------------------------------------------------------------
// BM_OptionsOneElementDefault       25.6 ns         25.6 ns     27332591
// BM_OptionsOneElementPresent       28.0 ns         28.0 ns     24999101

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
