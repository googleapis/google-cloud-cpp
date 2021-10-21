// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_RANDOM_DATABASE_NAME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_RANDOM_DATABASE_NAME_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/random.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

/// Create a random database name given a PRNG generator.
std::string RandomDatabaseName(google::cloud::internal::DefaultPRNG& generator,
                               std::chrono::system_clock::time_point tp =
                                   std::chrono::system_clock::now());

/// Return a regular expression (as a string) suitable to match the random
/// database IDs
std::string RandomDatabasePrefixRegex();

// The prefix for databases created on the (UTC) day at @p tp.
std::string RandomDatabasePrefix(std::chrono::system_clock::time_point tp);

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_RANDOM_DATABASE_NAME_H
