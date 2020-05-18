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

#include "google/cloud/pubsub/publisher_client.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/subscriber_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/optional.h"
#include "google/cloud/testing_util/example_driver.h"
#include <tuple>
#include <utility>

namespace {
std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator,
                          std::string const& prefix = "cloud-cpp-samples-") {
  constexpr int kMaxRandomTopicSuffixLength = 32;
  return prefix + google::cloud::internal::Sample(generator,
                                                  kMaxRandomTopicSuffixLength,
                                                  "abcdefghijklmnopqrstuvwxyz");
}

std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& prefix = "cloud-cpp-samples-") {
  constexpr int kMaxRandomSubscriptionSuffixLength = 32;
  return prefix + google::cloud::internal::Sample(
                      generator, kMaxRandomSubscriptionSuffixLength,
                      "abcdefghijklmnopqrstuvwxyz");
}

void CreateTopic(google::cloud::pubsub::PublisherClient client,
                 std::vector<std::string> const& argv) {
  //! [create-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::PublisherClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.CreateTopic(pubsub::CreateTopicBuilder(
        pubsub::Topic(std::move(project_id), std::move(topic_id))));
    if (!topic) throw std::runtime_error(topic.status().message());

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  //! [create-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopics(google::cloud::pubsub::PublisherClient client,
                std::vector<std::string> const& argv) {
  //! [list-topics]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::PublisherClient client, std::string const& project_id) {
    int count = 0;
    for (auto const& topic : client.ListTopics(project_id)) {
      if (!topic) throw std::runtime_error(topic.status().message());
      std::cout << "Topic Name: " << topic->name() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No topics found in project " << project_id << "\n";
    }
  }
  //! [list-topics]
  (std::move(client), argv.at(0));
}

void DeleteTopic(google::cloud::pubsub::PublisherClient client,
                 std::vector<std::string> const& argv) {
  //! [delete-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::PublisherClient client, std::string const& project_id,
     std::string const& topic_id) {
    auto status = client.DeleteTopic(
        pubsub::Topic(std::move(project_id), std::move(topic_id)));
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The topic was successfully deleted\n";
  }
  //! [delete-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateSubscription(google::cloud::pubsub::SubscriberClient client,
                        std::vector<std::string> const& argv) {
  //! [create-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriberClient client, std::string const& project_id,
     std::string const& topic_id, std::string const& subscription_id) {
    auto subscription =
        client.CreateSubscription(pubsub::CreateSubscriptionBuilder(
            pubsub::Subscription(project_id, std::move(subscription_id)),
            pubsub::Topic(project_id, std::move(topic_id))));
    if (!subscription)
      throw std::runtime_error(subscription.status().message());

    std::cout << "The subscription was successfully created: "
              << subscription->DebugString() << "\n";
  }
  //! [create-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ListSubscriptions(google::cloud::pubsub::SubscriberClient client,
                       std::vector<std::string> const& argv) {
  //! [list-subscriptions]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriberClient client, std::string const& project_id) {
    int count = 0;
    for (auto const& subscription : client.ListSubscriptions(project_id)) {
      if (!subscription)
        throw std::runtime_error(subscription.status().message());
      std::cout << "Subscription Name: " << subscription->name() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No subscriptions found in project " << project_id << "\n";
    }
  }
  //! [list-subscriptions]
  (std::move(client), argv.at(0));
}

void DeleteSubscription(google::cloud::pubsub::SubscriberClient client,
                        std::vector<std::string> const& argv) {
  //! [delete-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriberClient client, std::string const& project_id,
     std::string const& subscription_id) {
    auto status = client.DeleteSubscription(pubsub::Subscription(
        std::move(project_id), std::move(subscription_id)));
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The subscription was successfully deleted\n";
  }
  //! [delete-subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto topic_id = RandomTopicId(generator);
  auto subscription_id = RandomSubscriptionId(generator);

  google::cloud::pubsub::PublisherClient publisher_client(
      google::cloud::pubsub::MakePublisherConnection());
  google::cloud::pubsub::SubscriberClient subscriber_client(
      google::cloud::pubsub::MakeSubscriberConnection());

  std::cout << "\nRunning CreateTopic() sample\n";
  CreateTopic(publisher_client, {project_id, topic_id});

  std::cout << "\nRunning ListTopics() sample\n";
  ListTopics(publisher_client, {project_id});

  std::cout << "\nRunning CreateSubscription() sample\n";
  CreateSubscription(subscriber_client,
                     {project_id, topic_id, subscription_id});

  std::cout << "\nRunning ListSubscriptions() sample\n";
  ListSubscriptions(subscriber_client, {project_id});

  std::cout << "\nRunning DeleteSubscription() sample\n";
  DeleteSubscription(subscriber_client, {project_id, subscription_id});

  std::cout << "\nRunning delete-topic sample\n";
  DeleteTopic(publisher_client, {project_id, topic_id});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::pubsub::examples::CreatePublisherCommand;
  using ::google::cloud::pubsub::examples::CreateSubscriberCommand;
  using ::google::cloud::testing_util::Example;

  Example example({
      CreatePublisherCommand("create-topic", {"project-id", "topic-id"},
                             CreateTopic),
      CreatePublisherCommand("list-topics", {"project-id"}, ListTopics),
      CreatePublisherCommand("delete-topic", {"project-id", "topic-id"},
                             DeleteTopic),
      CreateSubscriberCommand("create-subscription",
                              {"project-id", "topic-id", "subscription-id"},
                              CreateSubscription),
      CreateSubscriberCommand("list-subscriptions", {"project-id"},
                              ListSubscriptions),
      CreateSubscriberCommand("delete-subscription",
                              {"project-id", "subscription-id"},
                              DeleteSubscription),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
