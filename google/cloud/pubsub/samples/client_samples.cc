// Copyright 2022 Google LLC
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

#include "google/cloud/pubsub/blocking_publisher.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription_builder.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <fstream>
#include <string>
#include <vector>

namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::pubsub::examples::RandomSubscriptionId;
using ::google::cloud::pubsub::examples::RandomTopicId;

void PublisherSetEndpoint(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 3) {
    throw examples::Usage{"publisher-set-endpoint <project-id> <topic-id>"};
  }
  //! [publisher-set-endpoint]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::Options;
  [](std::string const& project_id, std::string const& topic_id) {
    // This service supports specifying a regional or locational endpoint prefix
    // when creating the SubscriptionAdminConnection.
    // For example, to connect to "europe-central2-pubsub.googleapis.com":
    auto pub = pubsub::Publisher(pubsub::MakePublisherConnection(
        "europe-central2", pubsub::Topic(project_id, topic_id)));

    // This configuration is common with Private Google Access:
    //     https://cloud.google.com/vpc/docs/private-google-access
    auto vpc_pub = pubsub::Publisher(pubsub::MakePublisherConnection(
        pubsub::Topic(project_id, topic_id),
        Options{}.set<google::cloud::EndpointOption>(
            "private.googleapis.com")));
  }
  //! [publisher-set-endpoint]
  (argv.at(0), argv.at(1));
}

void PublisherServiceAccountKey(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 3) {
    throw examples::Usage{
        "publisher-service-account <project-id> <topic-id> <keyfile>"};
  }
  //! [publisher-service-account]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::Options;
  [](std::string project_id, std::string topic_id, std::string const& keyfile) {
    auto is = std::ifstream(keyfile);
    is.exceptions(std::ios::badbit);
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    return pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeServiceAccountCredentials(contents))));
  }
  //! [publisher-service-account]
  (argv.at(0), argv.at(1), argv.at(2));
}

void SubscriberSetEndpoint(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "subscriber-set-endpoint <project-id> <subscription-id>"};
  }
  //! [subscriber-set-endpoint]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::Options;
  [](std::string const& project_id, std::string const& subscription_id) {
    // This service supports specifying a regional or locational endpoint prefix
    // when creating the SubscriptionAdminConnection.
    // For example, to connect to "europe-central2-pubsub.googleapis.com":
    auto sub = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        "europe-central2", pubsub::Subscription(project_id, subscription_id)));

    // This configuration is common with Private Google Access:
    //     https://cloud.google.com/vpc/docs/private-google-access
    auto vpc_sub = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(project_id, subscription_id),
        Options{}.set<google::cloud::EndpointOption>(
            "private.googleapis.com")));
  }
  //! [subscriber-set-endpoint]
  (argv.at(0), argv.at(1));
}

void SubscriberServiceAccountKey(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 3) {
    throw examples::Usage{
        "subscriber-service-account <project-id> <subscription-id> <keyfile>"};
  }
  //! [subscriber-service-account]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::Options;
  [](std::string project_id, std::string subscription_id,
     std::string const& keyfile) {
    auto is = std::ifstream(keyfile);
    is.exceptions(std::ios::badbit);
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    auto subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));
    return pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        std::move(subscription),
        Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeServiceAccountCredentials(contents))));
  }
  //! [subscriber-service-account]
  (argv.at(0), argv.at(1), argv.at(2));
}

void BlockingPublisherSetEndpoint(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (!argv.empty()) {
    throw examples::Usage{"blocking-publisher-set-endpoint"};
  }
  //! [blocking-publisher-set-endpoint]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::Options;
  []() {
    // This service supports specifying a regional or locational endpoint prefix
    // when creating the SubscriptionAdminConnection.
    // For example, to connect to "europe-central2-pubsub.googleapis.com":
    auto pub = pubsub::BlockingPublisher(
        pubsub::MakeBlockingPublisherConnection("europe-central2"));

    // This configuration is common with Private Google Access:
    //     https://cloud.google.com/vpc/docs/private-google-access
    auto vpc_pub =
        pubsub::BlockingPublisher(pubsub::MakeBlockingPublisherConnection(
            Options{}.set<google::cloud::EndpointOption>(
                "private.googleapis.com")));
  }
  //! [blocking-publisher-set-endpoint]
  ();
}

void BlockingPublisherServiceAccountKey(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 1 || argv[0] == "--help") {
    throw examples::Usage{"blocking-publisher-service-account <keyfile>"};
  }
  //! [blocking-publisher-service-account]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::Options;
  [](std::string const& keyfile) {
    auto is = std::ifstream(keyfile);
    is.exceptions(std::ios::badbit);
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    return pubsub::BlockingPublisher(pubsub::MakeBlockingPublisherConnection(
        Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeServiceAccountCredentials(contents))));
  }
  //! [blocking-publisher-service-account]
  (argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE",
  });
  auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto keyfile =
      GetEnv("GOOGLE_CLOUD_CPP_TEST_SERVICE_ACCOUNT_KEYFILE").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const subscription_id = RandomSubscriptionId(generator);

  std::cout << "\nRunning PublisherSetEndpoint() sample" << std::endl;
  PublisherSetEndpoint({project_id, topic_id});

  std::cout << "\nRunning PublisherServiceAccountKey() sample" << std::endl;
  PublisherServiceAccountKey({project_id, topic_id, keyfile});

  std::cout << "\nRunning SubscriberSetEndpoint() sample" << std::endl;
  SubscriberSetEndpoint({project_id, subscription_id});

  std::cout << "\nRunning SubscriberServiceAccountKey() sample" << std::endl;
  SubscriberServiceAccountKey({project_id, subscription_id, keyfile});

  std::cout << "\nRunning BlockingPublisherSetEndpoint() sample" << std::endl;
  BlockingPublisherSetEndpoint({});

  std::cout << "\nRunning BlockingPublisherServiceAccountKey() sample"
            << std::endl;
  BlockingPublisherServiceAccountKey({keyfile});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::Example example({
      {"publisher-set-endpoint", PublisherSetEndpoint},
      {"publisher-service-account-key", PublisherServiceAccountKey},
      {"subscriber-set-endpoint", SubscriberSetEndpoint},
      {"subscriber-service-account-key", SubscriberServiceAccountKey},
      {"blocking-publisher-set-endpoint", BlockingPublisherSetEndpoint},
      {"blocking-publisher-service-account-key",
       BlockingPublisherServiceAccountKey},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
