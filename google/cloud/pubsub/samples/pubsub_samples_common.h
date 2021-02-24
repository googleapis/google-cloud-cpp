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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SAMPLES_PUBSUB_SAMPLES_COMMON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SAMPLES_PUBSUB_SAMPLES_COMMON_H

#include "google/cloud/pubsub/experimental/schema_admin_client.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/testing_util/example_driver.h"

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

using TopicAdminCommand = std::function<void(pubsub::TopicAdminClient,
                                             std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreateTopicAdminCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    TopicAdminCommand const& command);

using SubscriptionAdminCommand = std::function<void(
    pubsub::SubscriptionAdminClient, std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type
CreateSubscriptionAdminCommand(std::string const& name,
                               std::vector<std::string> const& arg_names,
                               SubscriptionAdminCommand const& command);

using SchemaAdminCommand = std::function<void(
    pubsub_experimental::SchemaAdminClient, std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreateSchemaAdminCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    SchemaAdminCommand const& command);

bool UsingEmulator();

}  // namespace examples
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SAMPLES_PUBSUB_SAMPLES_COMMON_H
