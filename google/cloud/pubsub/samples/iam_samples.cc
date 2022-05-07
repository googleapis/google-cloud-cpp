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

#include "google/cloud/iam/iam_policy_client.h"
#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/subscription_builder.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <sstream>
#include <utility>

namespace {

void GetTopicPolicy(std::vector<std::string> const& argv) {
  // [START pubsub_get_topic_policy]
  namespace iam = google::cloud::iam;
  namespace pubsub = google::cloud::pubsub;
  [](std::string project_id, std::string topic_id) {
    auto const topic =
        pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto client = iam::IAMPolicyClient(
        iam::MakeIAMPolicyConnection(pubsub::IAMPolicyOptions()));
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(topic.FullName());

    auto response = client.GetIamPolicy(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Policy for topic " << topic.FullName() << ": "
              << response->DebugString() << "\n";
  }
  // [END pubsub_get_topic_policy]
  (argv.at(0), argv.at(1));
}

void SetTopicPolicy(std::vector<std::string> const& argv) {
  // [START pubsub_set_topic_policy]
  namespace iam = google::cloud::iam;
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::StatusCode;
  [](std::string project_id, std::string topic_id) {
    auto const topic =
        pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto client = iam::IAMPolicyClient(
        iam::MakeIAMPolicyConnection(pubsub::IAMPolicyOptions()));

    // In production code, consider an OCC loop to handle concurrent changes
    // to the policy.
    google::iam::v1::GetIamPolicyRequest get;
    get.set_resource(topic.FullName());
    auto policy = client.GetIamPolicy(get);
    if (!policy) throw std::runtime_error(policy.status().message());

    google::iam::v1::SetIamPolicyRequest set;
    set.set_resource(topic.FullName());
    *set.mutable_policy() = *std::move(policy);
    // Add all users as viewers.
    auto& b0 = *set.mutable_policy()->add_bindings();
    b0.set_role("roles/pubsub.viewer");
    b0.add_members("domain:google.com");
    // Add a group as an editor.
    auto& b1 = *set.mutable_policy()->add_bindings();
    b1.set_role("roles/pubsub.publisher");
    b1.add_members("group:cloud-logs@google.com");

    auto response = client.SetIamPolicy(set);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Policy for topic " << topic.FullName() << ": "
              << response->DebugString() << "\n";
  }
  // [END pubsub_set_topic_policy]
  (argv.at(0), argv.at(1));
}

void TestTopicPermissions(std::vector<std::string> const& argv) {
  // [START pubsub_test_topic_permissions]
  namespace iam = google::cloud::iam;
  namespace pubsub = google::cloud::pubsub;
  [](std::string project_id, std::string topic_id) {
    auto const topic =
        pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto client = iam::IAMPolicyClient(
        iam::MakeIAMPolicyConnection(pubsub::IAMPolicyOptions()));
    google::iam::v1::TestIamPermissionsRequest request;
    request.set_resource(topic.FullName());
    request.add_permissions("pubsub.topics.publish");
    request.add_permissions("pubsub.topics.update");

    auto response = client.TestIamPermissions(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Allowed permissions for topic " << topic.FullName() << ":";
    for (auto const& permission : response->permissions()) {
      std::cout << " " << permission;
    }
    std::cout << "\n";
  }
  // [END pubsub_test_topic_permissions]
  (argv.at(0), argv.at(1));
}

void GetSubscriptionPolicy(std::vector<std::string> const& argv) {
  // [START pubsub_get_subscription_policy]
  namespace iam = google::cloud::iam;
  namespace pubsub = google::cloud::pubsub;
  [](std::string project_id, std::string subscription_id) {
    auto const subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));
    auto client = iam::IAMPolicyClient(
        iam::MakeIAMPolicyConnection(pubsub::IAMPolicyOptions()));
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(subscription.FullName());

    auto response = client.GetIamPolicy(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Policy for subscription " << subscription.FullName() << ": "
              << response->DebugString() << "\n";
  }
  // [END pubsub_get_subscription_policy]
  (argv.at(0), argv.at(1));
}

void SetSubscriptionPolicy(std::vector<std::string> const& argv) {
  // [START pubsub_set_subscription_policy]
  namespace iam = google::cloud::iam;
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::StatusCode;
  [](std::string project_id, std::string subscription_id) {
    auto const subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));
    auto client = iam::IAMPolicyClient(
        iam::MakeIAMPolicyConnection(pubsub::IAMPolicyOptions()));

    // In production code, consider an OCC loop to handle concurrent changes
    // to the policy.
    google::iam::v1::GetIamPolicyRequest get;
    get.set_resource(subscription.FullName());
    auto policy = client.GetIamPolicy(get);
    if (!policy) throw std::runtime_error(policy.status().message());

    google::iam::v1::SetIamPolicyRequest set;
    set.set_resource(subscription.FullName());
    *set.mutable_policy() = *std::move(policy);
    // Add all users as viewers.
    auto& b0 = *set.mutable_policy()->add_bindings();
    b0.set_role("roles/pubsub.viewer");
    b0.add_members("domain:google.com");
    // Add a group as an editor.
    auto& b1 = *set.mutable_policy()->add_bindings();
    b1.set_role("roles/editor");
    b1.add_members("group:cloud-logs@google.com");

    auto response = client.SetIamPolicy(set);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Policy for subscription " << subscription.FullName() << ": "
              << response->DebugString() << "\n";
  }
  // [END pubsub_set_subscription_policy]
  (argv.at(0), argv.at(1));
}

void TestSubscriptionPermissions(std::vector<std::string> const& argv) {
  // [START pubsub_test_subscription_permissions]
  namespace iam = google::cloud::iam;
  namespace pubsub = google::cloud::pubsub;
  [](std::string project_id, std::string subscription_id) {
    auto const subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));
    auto client = iam::IAMPolicyClient(
        iam::MakeIAMPolicyConnection(pubsub::IAMPolicyOptions()));
    google::iam::v1::TestIamPermissionsRequest request;
    request.set_resource(subscription.FullName());
    request.add_permissions("pubsub.subscriptions.consume");
    request.add_permissions("pubsub.subscriptions.update");
    std::cout << "request=" << request.DebugString() << "\n";

    auto response = client.TestIamPermissions(request);
    if (!response) throw std::runtime_error(response.status().message());
    std::cout << "Allowed permissions for subscription "
              << subscription.FullName() << ":";
    for (auto const& permission : response->permissions()) {
      std::cout << " " << permission;
    }
    std::cout << "\n";
  }
  // [END pubsub_test_topic_permissions]
  (argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::pubsub::examples::RandomSubscriptionId;
  using ::google::cloud::pubsub::examples::RandomTopicId;

  if (!argv.empty()) throw examples::Usage{"auto"};

  // IAM operations do not work in the emulator.
  if (google::cloud::pubsub::examples::UsingEmulator()) return;

  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const subscription_id = RandomSubscriptionId(generator);
  auto topic_admin_client =
      pubsub::TopicAdminClient(pubsub::MakeTopicAdminConnection());
  auto subscription_admin_client = pubsub::SubscriptionAdminClient(
      pubsub::MakeSubscriptionAdminConnection());

  std::cout << "\nCreate topic (" << topic_id << ")" << std::endl;
  auto topic = topic_admin_client
                   .CreateTopic(pubsub::TopicBuilder(
                       pubsub::Topic(project_id, topic_id)))
                   .value();

  std::cout << "\nCreate subscription (" << subscription_id << ")" << std::endl;
  auto subscription =
      subscription_admin_client
          .CreateSubscription(pubsub::Topic(project_id, topic_id),
                              pubsub::Subscription(project_id, subscription_id))
          .value();

  std::cout << "\nRunning GetTopicPolicy() sample" << std::endl;
  GetTopicPolicy({project_id, topic_id});

  std::cout << "\nRunning SetTopicPolicy() sample" << std::endl;
  try {
    SetTopicPolicy({project_id, topic_id});
  } catch (std::runtime_error const&) {
    // Ignore errors in this test because SetIamPolicy is flaky without an OCC
    // loop, and we do not want to complicate the example with an OCC loop.
  }

  std::cout << "\nRunning TestTopicPermissions() sample" << std::endl;
  TestTopicPermissions({project_id, topic_id});

  std::cout << "\nRunning GetSubscriptionPolicy() sample" << std::endl;
  GetSubscriptionPolicy({project_id, subscription_id});

  std::cout << "\nRunning SetSubscriptionPolicy() sample" << std::endl;
  try {
    SetSubscriptionPolicy({project_id, subscription_id});
  } catch (std::runtime_error const&) {
    // Ignore errors in this test because SetIamPolicy is flaky without an OCC
    // loop, and we do not want to complicate the example with an OCC loop.
  }

  std::cout << "\nRunning TestSubscriptionPermissions() sample" << std::endl;
  TestSubscriptionPermissions({project_id, subscription_id});

  std::cout << "\nCleanup subscription" << std::endl;
  (void)subscription_admin_client.DeleteSubscription(
      pubsub::Subscription(project_id, subscription_id));

  std::cout << "\nCleanup topic" << std::endl;
  (void)topic_admin_client.DeleteTopic(pubsub::Topic(project_id, topic_id));

  std::cout << "\nAutoRun done" << std::endl;
}

using IamCommand = std::function<void(std::vector<std::string> const&)>;

google::cloud::testing_util::Commands::value_type CreateIamCommand(
    std::string const& name, std::vector<std::string> const& arg_names,
    IamCommand const& command) {
  auto adapter = [=](std::vector<std::string> const& argv) {
    auto constexpr kFixedArguments = 1;
    if ((argv.size() == 2 && argv[0] == "--help") ||
        argv.size() != arg_names.size() + kFixedArguments) {
      std::ostringstream os;
      os << name << " <project-id>";
      for (auto const& a : arg_names) {
        os << " <" << a << ">";
      }
      throw google::cloud::testing_util::Usage{std::move(os).str()};
    }
    command(argv);
  };
  return google::cloud::testing_util::Commands::value_type{name,
                                                           std::move(adapter)};
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateIamCommand("get-topic-policy", {"topic-id"}, GetTopicPolicy),
      CreateIamCommand("set-topic-policy", {"topic-id"}, SetTopicPolicy),
      CreateIamCommand("test-topic-permissions", {"topic-id"},
                       TestTopicPermissions),
      CreateIamCommand("get-subscription-policy", {"subscription-id"},
                       GetSubscriptionPolicy),
      CreateIamCommand("set-subscription-policy", {"subscription-id"},
                       SetSubscriptionPolicy),
      CreateIamCommand("test-subscription-permissions", {"subscription-id"},
                       TestSubscriptionPermissions),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
