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
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include <regex>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
namespace examples {

bool UsingEmulator() {
  auto emulator =
      google::cloud::internal::GetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT");
  if (emulator) return true;
  return google::cloud::internal::GetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT")
      .has_value();
}

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen) {
  return google::cloud::storage::testing::MakeRandomBucketName(
      gen, "cloud-cpp-testing-examples");
}

std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix) {
  return prefix + testing::MakeRandomObjectName(gen);
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
      for (auto const& a : arg_names) {
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
