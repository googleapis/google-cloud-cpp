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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_RANDOM_NAMES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_RANDOM_NAMES_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/random.h"
#include <string>

namespace google {
namespace cloud {
namespace pubsub_testing {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/// Generate a random topic ID.
std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator,
                          std::string const& prefix = {});

/// Generate a random subscription ID.
std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& prefix = {});

/// Generate a random snapshot ID.
std::string RandomSnapshotId(google::cloud::internal::DefaultPRNG& generator,
                             std::string const& prefix = {});

/// Generate a random schema ID.
std::string RandomSchemaId(google::cloud::internal::DefaultPRNG& generator,
                           std::string const& prefix = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TESTING_RANDOM_NAMES_H
