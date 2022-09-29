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

auto constexpr kMessageSize = 128 * 1024 * 1024;

class GenerateBoundaryFixture : public ::benchmark::Fixture {
 public:
  std::string MakeRandomString(int n) {
    std::lock_guard<std::mutex> lk(mu_);
    return google::cloud::internal::Sample(
        generator_, n,
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  }

  std::string const& message() const { return message_; }

 private:
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
  std::string message_ = google::cloud::internal::Sample(
      generator_, kMessageSize,
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789\n\t \r{}()[]/;:<>,.");
};

BENCHMARK_F(GenerateBoundaryFixture, GenerateBoundaryImplSlow)
(benchmark::State& state) {
  auto make_string = [this](int n) { return MakeRandomString(n); };

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        GenerateMessageBoundaryImplSlow(message(), make_string, 16, 8));
  }
}

BENCHMARK_F(GenerateBoundaryFixture, GenerateBoundaryImpl)
(benchmark::State& state) {
  auto make_string = [this](int n) { return MakeRandomString(n); };

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        GenerateMessageBoundaryImpl(message(), make_string, 16, 8));
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
