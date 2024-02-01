// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/admin/subscription_admin_client.h"
#include "google/cloud/pubsub/admin/topic_admin_client.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <sstream>

namespace {

using google::cloud::pubsub::examples::Cleanup;
using SubscriptionAdminCommand =
    std::function<void(google::cloud::pubsub_admin::SubscriptionAdminClient,
                       std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type
CreateSubscriptionAdminCommand(std::string const& name,
                               std::vector<std::string> const& arg_names,
                               SubscriptionAdminCommand const& command) {
  auto adapter = [=](std::vector<std::string> const& argv) {
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != arg_names.size()) {
      std::ostringstream os;
      os << name;
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    google::cloud::pubsub_admin::SubscriptionAdminClient client(
        google::cloud::pubsub_admin::MakeSubscriptionAdminConnection());
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

void CreateSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id) {
    auto status = client.DeleteSubscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The subscription was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The subscription was successfully deleted\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::pubsub::examples::RandomSubscriptionId;
  using ::google::cloud::pubsub::examples::RandomTopicId;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const topic = google::cloud::pubsub::Topic(project_id, topic_id);
  auto const subscription_id = RandomSubscriptionId(generator);
  auto const subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);

  auto topic_admin_client = google::cloud::pubsub_admin::TopicAdminClient(
      google::cloud::pubsub_admin::MakeTopicAdminConnection());
  google::cloud::pubsub_admin::SubscriptionAdminClient
      subscription_admin_client(
          google::cloud::pubsub_admin::MakeSubscriptionAdminConnection());

  std::cout << "\nCreate topic (" << topic_id << ")" << std::endl;
  topic_admin_client.CreateTopic(topic.FullName());
  Cleanup cleanup;
  cleanup.Defer([topic_admin_client, topic]() mutable {
    (void)topic_admin_client.DeleteTopic(topic.FullName());
  });

  std::cout << "\nRunning CreateSubscription() sample" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, topic_id, subscription_id});

  cleanup.Defer(
      [subscription_admin_client, project_id, subscription_id]() mutable {
        std::cout << "\nRunning DeleteSubscription() sample" << std::endl;
        DeleteSubscription(subscription_admin_client,
                           {project_id, subscription_id});
      });

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateSubscriptionAdminCommand(
          "create-subscription", {"project-id", "topic-id", "subscription-id"},
          CreateSubscription),
      CreateSubscriptionAdminCommand("delete-subscription",
                                     {"project-id", "subscription-id"},
                                     DeleteSubscription),
      {"auto", AutoRun},
  });

  return example.Run(argc, argv);
}
