// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H_

#include "google/cloud/internal/random.h"
#include <chrono>
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace storage_benchmarks {
/**
 * Create a random bucket name.
 *
 * Most benchmarks need to create a bucket to storage their data. Using a random
 * bucket name makes it possible to run different instances of the benchmark
 * without interacting with previous or concurrent instances.
 */
std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix);

/// Create a random object name.
std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen);

/// Create a random chunk of data of a prescribed size.
std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size);

constexpr std::int64_t kKiB = 1024;
constexpr std::int64_t kMiB = 1024 * kKiB;
constexpr std::int64_t kGiB = 1024 * kMiB;
constexpr std::int64_t kTiB = 1024 * kGiB;

constexpr std::int64_t kKB = 1000;
constexpr std::int64_t kMB = 1000 * kKB;
constexpr std::int64_t kGB = 1000 * kMB;
constexpr std::int64_t kTB = 1000 * kGB;

/// Parse a string as a byte size, with support for unit suffixes.
std::int64_t ParseSize(std::string const& val);

/// Parse a string as a duration in seconds, with support for a unit suffixes.
std::chrono::seconds ParseDuration(std::string const& val);

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BENCHMARKS_BENCHMARK_UTILS_H_
