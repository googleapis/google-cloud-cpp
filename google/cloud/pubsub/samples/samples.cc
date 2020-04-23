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
#include "google/cloud/pubsub/subscriber_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/optional.h"
#include <sstream>
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

//! [create-topic]
void CreateTopic(google::cloud::pubsub::PublisherClient client,
                 std::string project_id, std::string topic_id) {
  namespace pubsub = google::cloud::pubsub;
  auto topic = client.CreateTopic(pubsub::CreateTopicBuilder(
      pubsub::Topic(std::move(project_id), std::move(topic_id))));
  if (!topic) throw std::runtime_error(topic.status().message());

  std::cout << "The topic was successfully created: " << topic->DebugString()
            << "\n";
}
//! [create-topic]

void CreateTopicCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("create-topic <project-id> <topic-id>");
  }
  google::cloud::pubsub::PublisherClient client(
      google::cloud::pubsub::MakePublisherConnection());
  CreateTopic(std::move(client), argv[0], argv[1]);
}

//! [list-topics]
void ListTopics(google::cloud::pubsub::PublisherClient client,
                std::string const& project_id) {
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

void ListTopicsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("create-topic <project-id>");
  }
  google::cloud::pubsub::PublisherClient client(
      google::cloud::pubsub::MakePublisherConnection());
  ListTopics(std::move(client), argv[0]);
}

//! [delete-topic]
void DeleteTopic(google::cloud::pubsub::PublisherClient client,
                 std::string project_id, std::string topic_id) {
  namespace pubsub = google::cloud::pubsub;
  auto status = client.DeleteTopic(
      pubsub::Topic(std::move(project_id), std::move(topic_id)));
  if (!status.ok()) throw std::runtime_error(status.message());

  std::cout << "The topic was successfully deleted\n";
}
//! [delete-topic]

void DeleteTopicCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("delete-topic <project-id> <topic-id>");
  }
  google::cloud::pubsub::PublisherClient client(
      google::cloud::pubsub::MakePublisherConnection());
  DeleteTopic(std::move(client), argv[0], argv[1]);
}

//! [create-subscription]
void CreateSubscription(google::cloud::pubsub::SubscriberClient client,
                        std::string const& project_id, std::string topic_id,
                        std::string subscription_id) {
  namespace pubsub = google::cloud::pubsub;
  auto subscription =
      client.CreateSubscription(pubsub::CreateSubscriptionBuilder(
          pubsub::Subscription(project_id, std::move(subscription_id)),
          pubsub::Topic(project_id, std::move(topic_id))));
  if (!subscription) throw std::runtime_error(subscription.status().message());

  std::cout << "The subscription was successfully created: "
            << subscription->DebugString() << "\n";
}
//! [create-subscription]

void CreateSubscriptionCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-subscription <project-id> <topic-id> <subscription-id>");
  }
  google::cloud::pubsub::SubscriberClient client(
      google::cloud::pubsub::MakeSubscriberConnection());
  CreateSubscription(std::move(client), argv[0], argv[1], argv[2]);
}

//! [list-subscriptions]
void ListSubscriptions(google::cloud::pubsub::SubscriberClient client,
                       std::string const& project_id) {
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

void ListSubscriptionsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("create-subscription <project-id>");
  }
  google::cloud::pubsub::SubscriberClient client(
      google::cloud::pubsub::MakeSubscriberConnection());
  ListSubscriptions(std::move(client), argv[0]);
}

//! [delete-subscription]
void DeleteSubscription(google::cloud::pubsub::SubscriberClient client,
                        std::string project_id, std::string subscription_id) {
  namespace pubsub = google::cloud::pubsub;
  auto status = client.DeleteSubscription(
      pubsub::Subscription(std::move(project_id), std::move(subscription_id)));
  if (!status.ok()) throw std::runtime_error(status.message());

  std::cout << "The subscription was successfully deleted\n";
}
//! [delete-subscription]

void DeleteSubscriptionCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "delete-subscription <project-id> <subscription-id>");
  }
  google::cloud::pubsub::SubscriberClient client(
      google::cloud::pubsub::MakeSubscriberConnection());
  DeleteSubscription(std::move(client), argv[0], argv[1]);
}

int RunOneCommand(std::vector<std::string> argv) {
  using CommandType = std::function<void(std::vector<std::string> const&)>;
  using CommandMap = std::map<std::string, CommandType>;

  CommandMap commands = {
      {"create-topic", CreateTopicCommand},
      {"list-topics", ListTopicsCommand},
      {"delete-topic", DeleteTopicCommand},
      {"create-subscription", CreateSubscriptionCommand},
      {"list-subscriptions", ListSubscriptionsCommand},
      {"delete-subscription", DeleteSubscriptionCommand},
  };

  static std::string usage_msg = [&argv, &commands] {
    auto last_slash = std::string(argv[0]).find_last_of("/\\");
    auto program = argv[0].substr(last_slash + 1);
    std::string usage;
    usage += "Usage: ";
    usage += program;
    usage += " <command> [arguments]\n\n";
    usage += "Commands:\n";
    for (auto&& kv : commands) {
      try {
        kv.second({});
      } catch (std::exception const& ex) {
        usage += "    ";
        usage += ex.what();
        usage += "\n";
      }
    }
    return usage;
  }();

  if (argv.size() < 2) {
    std::cerr << "Missing command argument\n" << usage_msg << "\n";
    return 1;
  }
  std::string command_name = argv[1];
  argv.erase(argv.begin());  // remove the program name from the list.
  argv.erase(argv.begin());  // remove the command name from the list.

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    std::cerr << "Unknown command " << command_name << "\n"
              << usage_msg << "\n";
    return 1;
  }

  // Run the command.
  command->second(argv);
  return 0;
}

void RunAll() {
  auto run_slow_integration_tests =
      google::cloud::internal::GetEnv("RUN_SLOW_INTEGRATION_TESTS")
          .value_or("");
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto topic_id = RandomTopicId(generator);
  auto subscription_id = RandomSubscriptionId(generator);

  std::cout << "\nRunning create-topic sample\n";
  RunOneCommand({"", "create-topic", project_id, topic_id});

  std::cout << "\nRunning list-topics sample\n";
  RunOneCommand({"", "list-topics", project_id});

  std::cout << "\nRunning create-subscription sample\n";
  RunOneCommand(
      {"", "create-subscription", project_id, topic_id, subscription_id});

  std::cout << "\nRunning list-subscriptions sample\n";
  RunOneCommand({"", "list-subscriptions", project_id});

  std::cout << "\nRunning delete-subscription sample\n";
  RunOneCommand({"", "delete-subscription", project_id, subscription_id});

  std::cout << "\nRunning delete-topic sample\n";
  RunOneCommand({"", "delete-topic", project_id, topic_id});
}

bool AutoRun() {
  return google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
             .value_or("") == "yes";
}

}  // namespace

int main(int ac, char* av[]) try {
  if (AutoRun()) {
    RunAll();
    return 0;
  }

  return RunOneCommand({av, av + ac});
} catch (std::exception const& ex) {
  std::cerr << "Standard exception caught: " << ex.what() << "\n";
  return 1;
}
