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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <sstream>
#include <stdexcept>

namespace google {
namespace cloud {
namespace storage_benchmarks {
std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of this bucket name must be <= 63 characters,
  static std::size_t const kMaxBucketNameLength = 63;
  std::size_t const max_random_characters =
      kMaxBucketNameLength - prefix.size();
  return prefix + google::cloud::internal::Sample(
                      gen, static_cast<int>(max_random_characters),
                      "abcdefghijklmnopqrstuvwxyz012456789");
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen) {
  return google::cloud::internal::Sample(gen, 128,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789");
}

std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size) {
  std::string result;
  result.reserve(desired_size);

  // Create lines of 128 characters to start with, we can fill the remaining
  // characters at the end.
  constexpr int kLineSize = 128;
  auto gen_random_line = [&gen](std::size_t count) {
    return google::cloud::internal::Sample(gen, static_cast<int>(count - 1),
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "012456789"
                                           " - _ : /") +
           "\n";
  };
  while (result.size() + kLineSize < desired_size) {
    result += gen_random_line(kLineSize);
  }
  if (result.size() < desired_size) {
    result += gen_random_line(desired_size - result.size());
  }

  return result;
}

bool EndsWith(std::string const& val, std::string const& suffix) {
  auto pos = val.rfind(suffix);
  if (pos == std::string::npos) {
    return false;
  }
  return pos == val.size() - suffix.size();
}

// This parser does not validate the input fully, but it is good enough for our
// purposes.
std::int64_t ParseSize(std::string const& val) {
  long s = std::stol(val);
  if (EndsWith(val, "TiB")) {
    return s * kTiB;
  }
  if (EndsWith(val, "GiB")) {
    return s * kGiB;
  }
  if (EndsWith(val, "MiB")) {
    return s * kMiB;
  }
  if (EndsWith(val, "KiB")) {
    return s * kKiB;
  }
  if (EndsWith(val, "TB")) {
    return s * kTB;
  }
  if (EndsWith(val, "GB")) {
    return s * kGB;
  }
  if (EndsWith(val, "MB")) {
    return s * kMB;
  }
  if (EndsWith(val, "KB")) {
    return s * kKB;
  }
  return s;
}

// This parser does not validate the input fully, but it is good enough for our
// purposes.
std::chrono::seconds ParseDuration(std::string const& val) {
  long s = std::stol(val);
  if (EndsWith(val, "h")) {
    return s * std::chrono::seconds(std::chrono::hours(1));
  }
  if (EndsWith(val, "m")) {
    return s * std::chrono::seconds(std::chrono::minutes(1));
  }
  if (EndsWith(val, "s")) {
    return s * std::chrono::seconds(std::chrono::seconds(1));
  }
  return std::chrono::seconds(s);
}

}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google
