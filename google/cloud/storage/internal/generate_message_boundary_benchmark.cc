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

#include "google/cloud/storage/internal/generate_message_boundary.h"
#include "google/cloud/internal/random.h"
#include <benchmark/benchmark.h>
#include <mutex>
#include <random>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

// CXX=clang++ CC=clang bazel run -c opt --
//   //google/cloud/storage:internal_generate_message_boundary_benchmark
//   --benchmark_repetitions=100
//
// clang-format off
// Run on (96 X 2000 MHz CPU s)
// CPU Caches:
//  L1 Data 32 KiB (x48)
//  L1 Instruction 32 KiB (x48)
//  L2 Unified 1024 KiB (x48)
//  L3 Unified 39424 KiB (x2)
// ---------------------------------------------------------------------------------------------
// Benchmark                                                   Time             CPU   Iterations
// ---------------------------------------------------------------------------------------------
// ...
// GenerateBoundaryFixture/GenerateBoundary_mean        20478679 ns     20475235 ns          100
// GenerateBoundaryFixture/GenerateBoundary_median      20583724 ns     20580943 ns          100
// GenerateBoundaryFixture/GenerateBoundary_stddev        335315 ns       335459 ns          100
// GenerateBoundaryFixture/GenerateBoundary_cv              1.64 %          1.64 %           100
// ...
// GenerateBoundaryFixture/GenerateBoundaryOld_mean     20809894 ns     20806317 ns          100
// GenerateBoundaryFixture/GenerateBoundaryOld_median   20520133 ns     20517279 ns          100
// GenerateBoundaryFixture/GenerateBoundaryOld_stddev    1284277 ns      1284334 ns          100
// GenerateBoundaryFixture/GenerateBoundaryOld_cv           6.17 %          6.17 %           100
// ...
// GenerateBoundaryFixture/WorstCase_mean              100747489 ns    100727911 ns          100
// GenerateBoundaryFixture/WorstCase_median            101026913 ns    101006689 ns          100
// GenerateBoundaryFixture/WorstCase_stddev              1285934 ns      1285884 ns          100
// GenerateBoundaryFixture/WorstCase_cv                     1.28 %          1.28 %           100
// ...
// GenerateBoundaryFixture/BestCase_mean                 9584895 ns      9583080 ns          100
// GenerateBoundaryFixture/BestCase_median               9598452 ns      9597243 ns          100
// GenerateBoundaryFixture/BestCase_stddev                 90679 ns        90643 ns          100
// GenerateBoundaryFixture/BestCase_cv                      0.95 %          0.95 %           100
// clang-format on

auto constexpr kMessageSize = 128 * 1024 * 1024;

class GenerateBoundaryFixture : public ::benchmark::Fixture {
 public:
  std::string MakeRandomString(int n) {
    std::lock_guard<std::mutex> lk(mu_);
    return google::cloud::internal::Sample(generator_, n,
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "0123456789"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  }

  std::string GenerateCandidate() {
    std::lock_guard<std::mutex> lk(mu_);
    return GenerateMessageBoundaryCandidate(generator_);
  }

  std::string const& message() const { return message_; }

 private:
  std::string MakeMessage() {
    std::string all;
    int min = (std::numeric_limits<char>::min)();  // NOLINT
    int end = (std::numeric_limits<char>::max)() + 1;
    for (int c = min; c != end; ++c) all.push_back(static_cast<char>(c));
    return google::cloud::internal::Sample(generator_, kMessageSize, all);
  }

  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
  std::string message_ = MakeMessage();
};

BENCHMARK_F(GenerateBoundaryFixture, GenerateBoundary)
(benchmark::State& state) {
  auto make_string = [this]() { return GenerateCandidate(); };

  for (auto _ : state) {
    benchmark::DoNotOptimize(GenerateMessageBoundary(message(), make_string));
  }
}

BENCHMARK_F(GenerateBoundaryFixture, GenerateBoundaryOld)
(benchmark::State& state) {
  auto old = [this](std::string const& message) {
    auto candidate = MakeRandomString(16);
    for (std::string::size_type i = message.find(candidate, 0);
         i != std::string::npos; i = message.find(candidate, i)) {
      candidate += MakeRandomString(8);
    }
    return candidate;
  };

  for (auto _ : state) {
    benchmark::DoNotOptimize(old(message()));
  }
}

BENCHMARK_F(GenerateBoundaryFixture, WorstCase)
(benchmark::State& state) {
  auto test = [](std::string const& message) {
    auto count = std::count(message.begin(), message.end(), 'Z');
    return std::string(count % 64, 'A');
  };

  for (auto _ : state) {
    benchmark::DoNotOptimize(test(message()));
  }
}

BENCHMARK_F(GenerateBoundaryFixture, BestCase)
(benchmark::State& state) {
  auto test = [](std::string const& message) {
    auto count = 0;
    for (std::size_t i = 0; i < message.size(); i += 64) {
      count += message[i] == 'Z' ? 1 : 0;
    }
    return std::string(count % 64, 'A');
  };

  for (auto _ : state) {
    benchmark::DoNotOptimize(test(message()));
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
