// Copyright 2020 Google LLC
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

#include "google/cloud/storage/testing/random_names.h"
#include "absl/time/civil_time.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
namespace storage {
namespace testing {

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of this bucket name must be <= 63 characters,
  static std::size_t const kMaxBucketNameLength = 63;
  auto const date =
      absl::FormatCivilTime(absl::ToCivilDay(absl::Now(), absl::UTCTimeZone()));
  auto const full = prefix + '-' + date + '_';
  std::size_t const max_random_characters = kMaxBucketNameLength - full.size();
  return full + google::cloud::internal::Sample(
                    gen, static_cast<int>(max_random_characters),
                    "abcdefghijklmnopqrstuvwxyz0123456789");
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen) {
  // GCS accepts object name up to 1024 characters, but 128 seems long enough to
  // avoid collisions.
  auto constexpr kObjectNameLength = 128;
  return google::cloud::internal::Sample(gen, kObjectNameLength,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789");
}

std::string MakeRandomFileName(google::cloud::internal::DefaultPRNG& gen) {
  // All the operating systems we support handle filenames with 28 characters,
  // they may support much longer names in fact, but 28 is good enough for our
  // purposes.
  auto constexpr kFilenameLength = 28;
  return google::cloud::internal::Sample(gen, kFilenameLength,
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "0123456789") +
         ".txt";
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
                                           "0123456789"
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

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google
