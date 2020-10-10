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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_EXAMPLES_STORAGE_EXAMPLES_COMMON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_EXAMPLES_STORAGE_EXAMPLES_COMMON_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/remove_stale_buckets.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"

namespace google {
namespace cloud {
namespace storage {
namespace examples {

using ::google::cloud::storage::testing::RemoveBucketAndContents;
using ::google::cloud::storage::testing::RemoveStaleBuckets;
using ::google::cloud::testing_util::CheckEnvironmentVariablesAreSet;
using ::google::cloud::testing_util::Commands;
using ::google::cloud::testing_util::CommandType;
using ::google::cloud::testing_util::Example;
using ::google::cloud::testing_util::Usage;

bool UsingTestbench();

std::string MakeRandomBucketName(google::cloud::internal::DefaultPRNG& gen);
std::string MakeRandomObjectName(google::cloud::internal::DefaultPRNG& gen,
                                 std::string const& prefix);

using ClientCommand = std::function<void(google::cloud::storage::Client,
                                         std::vector<std::string> const& argv)>;

// If the last value of `arg_names` contains `...` it represents a variable
// number (0 or more) of arguments.
Commands::value_type CreateCommandEntry(
    std::string const& name, std::vector<std::string> const& arg_names,
    ClientCommand const& command);

}  // namespace examples
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_EXAMPLES_STORAGE_EXAMPLES_COMMON_H
