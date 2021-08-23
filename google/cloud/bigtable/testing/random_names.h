// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_RANDOM_NAMES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_RANDOM_NAMES_H

#include "google/cloud/internal/random.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/// Create a random table ID given a PRNG generator.
std::string RandomTableId(google::cloud::internal::DefaultPRNG& generator,
                          std::chrono::system_clock::time_point tp =
                              std::chrono::system_clock::now());

/// The prefix for tables created on the (UTC) day at @p tp.
std::string RandomTableId(std::chrono::system_clock::time_point tp);

/// Return a regular expression suitable to match the random table IDs
std::string RandomTableIdRegex();

/// Create a random backup ID given a PRNG generator.
std::string RandomBackupId(google::cloud::internal::DefaultPRNG& generator,
                           std::chrono::system_clock::time_point tp =
                               std::chrono::system_clock::now());

/// The prefix for backups created on the (UTC) day at @p tp.
std::string RandomBackupId(std::chrono::system_clock::time_point tp);

/// Return a regular expression suitable to match the random backup IDs
std::string RandomBackupIdRegex();

/// Create a random instance ID given a PRNG generator.
std::string RandomClusterId(google::cloud::internal::DefaultPRNG& generator,
                            std::chrono::system_clock::time_point tp =
                                std::chrono::system_clock::now());

/// The prefix for instances created on the (UTC) day at @p tp.
std::string RandomClusterId(std::chrono::system_clock::time_point tp);

/// Return a regular expression suitable to match the random instance IDs
std::string RandomClusterIdRegex();

/// Create a random instance ID given a PRNG generator.
std::string RandomInstanceId(google::cloud::internal::DefaultPRNG& generator,
                             std::chrono::system_clock::time_point tp =
                                 std::chrono::system_clock::now());

/// The prefix for instances created on the (UTC) day at @p tp.
std::string RandomInstanceId(std::chrono::system_clock::time_point tp);

/// Return a regular expression suitable to match the random instance IDs
std::string RandomInstanceIdRegex();

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_RANDOM_NAMES_H
