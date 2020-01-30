// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RANDOM_NAMES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RANDOM_NAMES_H

#include "google/cloud/internal/random.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
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

/// Create a random local filename.
std::string MakeRandomFileName(google::cloud::internal::DefaultPRNG& gen);

/// Create a random chunk of data of a prescribed size.
std::string MakeRandomData(google::cloud::internal::DefaultPRNG& gen,
                           std::size_t desired_size);

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_RANDOM_NAMES_H
