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

#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/internal/getenv.h"
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub {
namespace examples {

google::cloud::testing_util::Commands::value_type CreatePublisherCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    PublisherCommand const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 2;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != arg_names.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id> <topic-id>";
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    Topic const topic(argv.at(0), argv.at(1));
    argv.erase(argv.begin(), argv.begin() + 2);
    google::cloud::pubsub::Publisher client(
        google::cloud::pubsub::MakePublisherConnection(topic, {}));
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

google::cloud::testing_util::Commands::value_type CreateSubscriberCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    SubscriberCommand const& command) {
  auto adapter = [=](std::vector<std::string> argv) {
    auto constexpr kFixedArguments = 2;
    if ((argv.size() == 1 && argv[0] == "--help") ||
        argv.size() != arg_names.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id> <subscription-id>";
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    google::cloud::pubsub::Subscriber client(
        google::cloud::pubsub::MakeSubscriberConnection(
            pubsub::Subscription(argv.at(0), argv.at(1))));
    argv.erase(argv.begin(), argv.begin() + 2);
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

google::cloud::testing_util::Commands::value_type CreateTopicAdminCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    TopicAdminCommand const& command) {
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
    google::cloud::pubsub::TopicAdminClient client(
        google::cloud::pubsub::MakeTopicAdminConnection());
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

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
    google::cloud::pubsub::SubscriptionAdminClient client(
        google::cloud::pubsub::MakeSubscriptionAdminConnection());
    command(std::move(client), std::move(argv));
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

bool UsingEmulator() {
  return google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST").has_value();
}

}  // namespace examples
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
