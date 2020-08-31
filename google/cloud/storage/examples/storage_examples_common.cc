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

#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include "absl/time/civil_time.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
namespace examples {

bool UsingTestbench() {
  return !google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
              .value_or("")
              .empty();
}

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of a bucket name must be <= 63 characters,
  static std::size_t const kMaxBucketNameLength = 63;
  auto const date =
      absl::FormatCivilTime(absl::ToCivilDay(absl::Now(), absl::UTCTimeZone()));
  auto const full = prefix + date + "_";
  std::size_t const max_random_characters = kMaxBucketNameLength - full.size();
  // bucket names might also contain `-` and `_` characters, but we do not
  // *need* to use them.
  return full + google::cloud::internal::Sample(
                    gen, static_cast<int>(max_random_characters),
                    "abcdefghijklmnopqrstuvwxyz0123456789");
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  // The total length of an object name is something like 1024 characters (UTF-8
  // encoded), but we do not need that many.
  static std::size_t const kMaxObjectNameLength = 128;
  std::size_t const max_random_characters =
      kMaxObjectNameLength - prefix.size();
  // object names might contain all kinds of characters, but we can use just
  // a subset for these purposes.
  return prefix + google::cloud::internal::Sample(
                      gen, static_cast<int>(max_random_characters),
                      "abcdefghijklmnopqrstuvwxyz0123456789");
}

Commands::value_type CreateCommandEntry(
    std::string const& name, std::vector<std::string> const& arg_names,
    ClientCommand const& command) {
  bool allow_varargs =
      !arg_names.empty() && arg_names.back().find("...") != std::string::npos;
  auto adapter = [=](std::vector<std::string> const& argv) {
    if ((argv.size() == 1 && argv[0] == "--help") ||
        (allow_varargs ? argv.size() < (arg_names.size() - 1)
                       : argv.size() != arg_names.size())) {
      std::ostringstream os;
      os << name;
      for (auto& a : arg_names) {
        os << " " << a;
      }
      throw Usage{std::move(os).str()};
    }
    auto client = google::cloud::storage::Client::CreateDefaultClient().value();
    command(std::move(client), std::move(argv));
  };
  return {name, std::move(adapter)};
}

}  // namespace examples
}  // namespace storage
}  // namespace cloud
}  // namespace google
