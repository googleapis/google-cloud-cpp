// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SAMPLES_PUBSUB_SAMPLES_COMMON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SAMPLES_PUBSUB_SAMPLES_COMMON_H

#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/schema_client.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/testing_util/example_driver.h"
#include "absl/time/time.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
namespace examples {

using PublisherCommand =
    std::function<void(pubsub::Publisher, std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreatePublisherCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    PublisherCommand const& command);

using SubscriberCommand =
    std::function<void(pubsub::Subscriber, std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreateSubscriberCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    SubscriberCommand const& command);

using SchemaServiceCommand = std::function<void(
    pubsub::SchemaServiceClient, std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreateSchemaServiceCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    SchemaServiceCommand const& command);

bool UsingEmulator();

std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator);

std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator);

std::string RandomSnapshotId(google::cloud::internal::DefaultPRNG& generator);

std::string RandomSchemaId(google::cloud::internal::DefaultPRNG& generator);

std::string ReadFile(std::string const& path);

// Commit a schema with a revision and return the first and last revision ids.
std::pair<std::string, std::string> CommitSchemaWithRevisionsForTesting(
    google::cloud::pubsub::SchemaServiceClient& client,
    std::string const& project_id, std::string const& schema_id,
    std::string const& schema_file, std::string const& revised_schema_file,
    std::string const& type);

// Delete all schemas older than 48 hours. Ignore any failures. If multiple
// tests are cleaning up schemas in parallel, then the delete call might fail.
void CleanupSchemas(google::cloud::pubsub::SchemaServiceClient& schema_admin,
                    std::string const& project_id, absl::Time const& time_now);

class Cleanup {
 public:
  Cleanup() = default;
  ~Cleanup() {
    for (auto i = actions_.rbegin(); i != actions_.rend(); ++i) (*i)();
  }
  void Defer(std::function<void()> f) { actions_.push_back(std::move(f)); }

 private:
  std::vector<std::function<void()>> actions_;
};

}  // namespace examples
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SAMPLES_PUBSUB_SAMPLES_COMMON_H
