// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/crc32c.h"
#include <benchmark/benchmark.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Run on (128 X 2250 MHz CPU s)
// CPU Caches:
//   L1 Data 32 KiB (x64)
//   L1 Instruction 32 KiB (x64)
//   L2 Unified 512 KiB (x64)
//   L3 Unified 16384 KiB (x16)
// Load Average: 11.17, 4.97, 7.43
// -------------------------------------------------------------
// Benchmark                   Time             CPU   Iterations
// -------------------------------------------------------------
// BM_Crc32cDuplicate  192261696 ns    192256154 ns            4
// BM_Crc32cConcat      95538105 ns     95538296 ns            7

auto constexpr kMessage = 2 * 1024 * std::size_t{1024};
auto constexpr kWriteSize = 16 * kMessage;
auto constexpr kUploadSize = 8 * kWriteSize;

void BM_Crc32cDuplicate(benchmark::State& state) {
  auto buffer = std::string(kWriteSize, '0');
  auto crc = std::uint32_t{0};
  for (auto _ : state) {
    for (std::size_t offset = 0; offset < kUploadSize; offset += kWriteSize) {
      for (std::size_t m = 0; m < kWriteSize; m += kMessage) {
        auto w = absl::string_view{buffer}.substr(m, kMessage);
        auto c = Crc32c(w);
        benchmark::DoNotOptimize(c);
      }
      crc = ExtendCrc32c(crc, buffer, absl::nullopt);
    }
  }
  benchmark::DoNotOptimize(crc);
}
BENCHMARK(BM_Crc32cDuplicate);

void BM_Crc32cConcat(benchmark::State& state) {
  auto buffer = std::string(kWriteSize, '0');
  auto crc = std::uint32_t{0};
  for (auto _ : state) {
    for (std::size_t offset = 0; offset < kUploadSize; offset += kWriteSize) {
      for (std::size_t m = 0; m < kWriteSize; m += kMessage) {
        auto w = absl::string_view{buffer}.substr(m, kMessage);
        auto c = Crc32c(w);
        crc = ExtendCrc32c(crc, w, c);
      }
    }
  }
  benchmark::DoNotOptimize(crc);
}
BENCHMARK(BM_Crc32cConcat);

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
