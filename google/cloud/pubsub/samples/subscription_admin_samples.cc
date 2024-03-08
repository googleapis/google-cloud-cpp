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
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/snapshot.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/example_driver.h"
#include "absl/strings/match.h"
#include <sstream>

namespace {

using google::cloud::pubsub::examples::Cleanup;
using ::google::cloud::pubsub::examples::UsingEmulator;

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

// Delete all subscriptions created with that include "cloud-cpp-samples".
// Ignore any failures. If multiple tests are cleaning up subscriptions in
// parallel, then the delete call might fail.
void CleanupSubscriptions(
    google::cloud::pubsub_admin::SubscriptionAdminClient& client,
    std::string const& project_id, absl::Time time_now) {
  for (auto const& subscription : client.ListSubscriptions(
           google::cloud::Project(project_id).FullName())) {
    if (!subscription) continue;
    std::string const keyword = "cloud-cpp-samples-";
    auto iter = subscription->name().find(keyword);
    if (iter == std::string::npos) continue;
    // Extract the date from the resource name which is in the format
    // `*-cloud-cpp-samples-YYYY-MM-DD-`.
    auto date = subscription->name().substr(iter + keyword.size(), 10);
    absl::CivilDay day;
    if (absl::ParseCivilTime(date, &day) &&
        absl::FromCivil(day, absl::UTCTimeZone()) <
            time_now - absl::Hours(36)) {
      google::pubsub::v1::DeleteSubscriptionRequest request;
      request.set_subscription(subscription->name());
      (void)client.DeleteSubscription(request);
    }
  }
}

void CreateSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_pull_subscription]
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
  }
  // [END pubsub_create_pull_subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateBigQuerySubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_bigquery_subscription]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id, std::string const& table_id) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.mutable_bigquery_config()->set_table(table_id);
    auto sub = client.CreateSubscription(request);
    if (!sub) {
      if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
        std::cout << "The subscription already exists\n";
        return;
      }
      throw std::move(sub).status();
    }

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  // [END pubsub_create_bigquery_subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateCloudStorageSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_cloud_storage_subscription]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id, std::string const& bucket) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.mutable_cloud_storage_config()->set_bucket(bucket);
    auto sub = client.CreateSubscription(request);
    if (!sub) {
      if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
        std::cout << "The subscription already exists\n";
        return;
      }
      throw std::move(sub).status();
    }

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  // [END pubsub_create_cloud_storage_subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateDeadLetterSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_dead_letter_create_subscription]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id,
     std::string const& dead_letter_topic_id,
     int dead_letter_delivery_attempts) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.mutable_dead_letter_policy()->set_dead_letter_topic(
        pubsub::Topic(project_id, dead_letter_topic_id).FullName());
    request.mutable_dead_letter_policy()->set_max_delivery_attempts(
        dead_letter_delivery_attempts);
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";

    std::cout << "It will forward dead letter messages to: "
              << sub->dead_letter_policy().dead_letter_topic() << "\n";

    std::cout << "After " << sub->dead_letter_policy().max_delivery_attempts()
              << " delivery attempts.\n";
  }
  // [END pubsub_dead_letter_create_subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   std::stoi(argv.at(4)));
}

void CreateSubscriptionWithExactlyOnceDelivery(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_subscription_with_exactly_once_delivery]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.set_enable_exactly_once_delivery(true);
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  // [END pubsub_create_subscription_with_exactly_once_delivery]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateFilteredSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_subscription_with_filter]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string topic_id,
     std::string subscription_id) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, std::move(subscription_id))
            .FullName());
    request.set_topic(
        pubsub::Topic(project_id, std::move(topic_id)).FullName());
    request.set_filter(R"""(attributes.is-even = "false")""");
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  // [END pubsub_create_subscription_with_filter]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateOrderingSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_enable_subscription_ordering]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.set_enable_message_ordering(true);
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  // [END pubsub_enable_subscription_ordering]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreatePushSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_push_subscription]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id, std::string const& endpoint) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.mutable_push_config()->set_push_endpoint(endpoint);
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
    // [END pubsub_create_push_subscription]
  }(std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateUnwrappedPushSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_create_unwrapped_push_subscription]
  namespace pubsub = ::google::cloud::pubsub;
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& topic_id,
     std::string const& subscription_id, std::string const& endpoint) {
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(pubsub::Topic(project_id, topic_id).FullName());
    request.mutable_push_config()->set_push_endpoint(endpoint);
    request.mutable_push_config()->mutable_no_wrapper()->set_write_metadata(
        true);
    auto sub = client.CreateSubscription(request);
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  // [END pubsub_create_unwrapped_push_subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void UpdateDeadLetterSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_dead_letter_update_subscription]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id,
     std::string const& dead_letter_topic_id,
     int dead_letter_delivery_attempts) {
    google::pubsub::v1::UpdateSubscriptionRequest request;
    request.mutable_subscription()->set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.mutable_subscription()
        ->mutable_dead_letter_policy()
        ->set_dead_letter_topic(
            pubsub::Topic(project_id, dead_letter_topic_id).FullName());
    request.mutable_subscription()
        ->mutable_dead_letter_policy()
        ->set_max_delivery_attempts(dead_letter_delivery_attempts);
    *request.mutable_update_mask()->add_paths() = "dead_letter_policy";
    auto sub = client.UpdateSubscription(request);
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription has been updated to: " << sub->DebugString()
              << "\n";

    std::cout << "It will forward dead letter messages to: "
              << sub->dead_letter_policy().dead_letter_topic() << "\n";

    std::cout << "After " << sub->dead_letter_policy().max_delivery_attempts()
              << " delivery attempts.\n";
  }
  // [END pubsub_dead_letter_update_subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2),
   std::stoi(argv.at(3)));
}

void RemoveDeadLetterPolicy(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_dead_letter_remove]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id) {
    google::pubsub::v1::UpdateSubscriptionRequest request;
    request.mutable_subscription()->set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.mutable_subscription()->clear_dead_letter_policy();
    *request.mutable_update_mask()->add_paths() = "dead_letter_policy";
    auto sub = client.UpdateSubscription(request);
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription has been updated to: " << sub->DebugString()
              << "\n";
  }
  // [END pubsub_dead_letter_remove]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id) {
    google::pubsub::v1::GetSubscriptionRequest request;
    request.set_subscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    auto sub = client.GetSubscription(request);
    if (!sub) throw std::move(sub).status();

    std::cout << "The subscription exists and its metadata is: "
              << sub->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void UpdateSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id) {
    google::pubsub::v1::UpdateSubscriptionRequest request;
    request.mutable_subscription()->set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.mutable_subscription()->set_ack_deadline_seconds(60);
    *request.mutable_update_mask()->add_paths() = "ack_deadline_seconds";
    auto s = client.UpdateSubscription(request);
    if (!s) throw std::move(s).status();

    std::cout << "The subscription has been updated to: " << s->DebugString()
              << "\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void ListSubscriptions(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_list_subscriptions]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id) {
    int count = 0;
    google::pubsub::v1::ListSubscriptionsRequest request;
    request.set_project(google::cloud::Project(project_id).FullName());
    for (auto& subscription : client.ListSubscriptions(request)) {
      if (!subscription) throw std::move(subscription).status();
      std::cout << "Subscription Name: " << subscription->name() << "\n";
      ++count;
    }
    if (count == 0) {
      std::cout << "No subscriptions found in project " << project_id << "\n";
    }
  }
  // [END pubsub_list_subscriptions]
  (std::move(client), argv.at(0));
}

void ModifyPushConfig(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_update_push_configuration]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id,
     std::string const& endpoint) {
    google::pubsub::v1::ModifyPushConfigRequest request;
    request.set_subscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.mutable_push_config()->set_push_endpoint(endpoint);
    auto status = client.ModifyPushConfig(request);
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The subscription push configuration was successfully"
              << " modified\n";
  }
  // [END pubsub_update_push_configuration]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateSnapshot(google::cloud::pubsub_admin::SubscriptionAdminClient client,
                    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id,
     std::string const& snapshot_id) {
    google::pubsub::v1::CreateSnapshotRequest request;
    request.set_name(pubsub::Snapshot(project_id, snapshot_id).FullName());
    request.set_subscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    auto snapshot = client.CreateSnapshot(request);
    if (snapshot.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The snapshot already exists\n";
      return;
    }
    if (!snapshot.ok()) throw std::move(snapshot).status();

    std::cout << "The snapshot was successfully created: "
              << snapshot->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetSnapshot(google::cloud::pubsub_admin::SubscriptionAdminClient client,
                 std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& snapshot_id) {
    google::pubsub::v1::GetSnapshotRequest request;
    request.set_snapshot(pubsub::Snapshot(project_id, snapshot_id).FullName());
    auto snapshot = client.GetSnapshot(request);
    auto response = client.GetSnapshot(request);
    if (!response.ok()) throw std::move(response).status();

    std::cout << "The snapshot details are: " << response->DebugString()
              << "\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void UpdateSnapshot(google::cloud::pubsub_admin::SubscriptionAdminClient client,
                    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string snapshot_id) {
    google::pubsub::v1::UpdateSnapshotRequest request;
    request.mutable_snapshot()->set_name(
        pubsub::Snapshot(project_id, std::move(snapshot_id)).FullName());
    (*request.mutable_snapshot()->mutable_labels())["samples-cpp"] = "gcp";
    *request.mutable_update_mask()->add_paths() = "labels";

    auto snap = client.UpdateSnapshot(request);
    if (!snap.ok()) throw std::move(snap).status();

    std::cout << "The snapshot was successfully updated: "
              << snap->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void ListSnapshots(google::cloud::pubsub_admin::SubscriptionAdminClient client,
                   std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id) {
    std::cout << "Snapshot list for project " << project_id << ":\n";
    for (auto& snapshot :
         client.ListSnapshots(google::cloud::Project(project_id).FullName())) {
      if (!snapshot) throw std::move(snapshot).status();
      std::cout << "Snapshot Name: " << snapshot->name() << "\n";
    }
  }(std::move(client), argv.at(0));
}

void DeleteSnapshot(google::cloud::pubsub_admin::SubscriptionAdminClient client,
                    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& snapshot_id) {
    auto status = client.DeleteSnapshot(
        pubsub::Snapshot(project_id, snapshot_id).FullName());
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The snapshot was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The snapshot was successfully deleted\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void SeekWithSnapshot(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id,
     std::string const& snapshot_id) {
    google::pubsub::v1::SeekRequest request;
    request.set_subscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_snapshot(pubsub::Snapshot(project_id, snapshot_id).FullName());
    auto response = client.Seek(request);
    if (!response.ok()) throw std::move(response).status();

    std::cout << "The subscription seek was successful: "
              << response->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void SeekWithTimestamp(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub_admin::SubscriptionAdminClient client,
     std::string const& project_id, std::string const& subscription_id,
     std::string const& seconds) {
    google::pubsub::v1::SeekRequest request;
    request.set_subscription(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.mutable_time()->set_seconds(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count() -
        std::stoi(seconds));
    auto response = client.Seek(request);
    if (!response.ok()) throw std::move(response).status();

    std::cout << "The subscription seek was successful: "
              << response->DebugString() << "\n";
  }(std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteSubscription(
    google::cloud::pubsub_admin::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  // [START pubsub_delete_subscription]
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
    // [END pubsub_delete_subscription]
  }(std::move(client), argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::pubsub::examples::RandomSnapshotId;
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
  auto const bigquery_subscription_id = RandomSubscriptionId(generator);
  auto const cloud_storage_subscription_id = RandomSubscriptionId(generator);
  auto const dead_letter_topic_id = "dead-letter-" + RandomTopicId(generator);
  auto const dead_letter_topic =
      google::cloud::pubsub::Topic(project_id, dead_letter_topic_id);
  auto const dead_letter_subscription_id = RandomSubscriptionId(generator);

  auto const exactly_once_subscription_id = RandomSubscriptionId(generator);
  auto const filtered_subscription_id = RandomSubscriptionId(generator);
  auto const ordering_topic_id = "ordering-" + RandomTopicId(generator);
  auto const ordering_topic =
      google::cloud::pubsub::Topic(project_id, ordering_topic_id);
  auto const ordering_subscription_id = RandomSubscriptionId(generator);
  auto const push_subscription_id = RandomSubscriptionId(generator);
  auto const unwrapped_push_subscription_id = RandomSubscriptionId(generator);
  auto const snapshot_id = RandomSnapshotId(generator);

  auto topic_admin_client = google::cloud::pubsub_admin::TopicAdminClient(
      google::cloud::pubsub_admin::MakeTopicAdminConnection());
  google::cloud::pubsub_admin::SubscriptionAdminClient
      subscription_admin_client(
          google::cloud::pubsub_admin::MakeSubscriptionAdminConnection());

  using ::google::cloud::StatusCode;
  auto ignore_emulator_failures =
      [](auto lambda, StatusCode code = StatusCode::kUnimplemented) {
        try {
          lambda();
        } catch (google::cloud::Status const& s) {
          if (UsingEmulator() && s.code() == code) return;
          throw;
        } catch (...) {
          throw;
        }
      };

  // Delete subscriptions over 36 hours old.
  CleanupSubscriptions(subscription_admin_client, project_id,
                       absl::FromChrono(std::chrono::system_clock::now()));

  std::cout << "\nCreate topic (" << topic_id << ")" << std::endl;
  topic_admin_client.CreateTopic(topic.FullName());
  std::cout << "\nCreate topic (" << dead_letter_topic_id << ")" << std::endl;
  topic_admin_client.CreateTopic(dead_letter_topic.FullName());
  std::cout << "\nCreate topic (" << ordering_topic_id << ")" << std::endl;
  topic_admin_client.CreateTopic(ordering_topic.FullName());
  Cleanup cleanup;
  cleanup.Defer([topic_admin_client, topic, ordering_topic]() mutable {
    std::cout << "\nDelete topic (" << topic.topic_id() << ")" << std::endl;
    (void)topic_admin_client.DeleteTopic(topic.FullName());
    std::cout << "\nDelete topic (" << ordering_topic.topic_id() << ")"
              << std::endl;
    (void)topic_admin_client.DeleteTopic(ordering_topic.FullName());
  });

  std::cout << "\nRunning CreateSubscription() [1] sample" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, topic_id, subscription_id});
  cleanup.Defer(
      [subscription_admin_client, project_id, subscription_id]() mutable {
        std::cout << "\nRunning DeleteSubscription() sample" << std::endl;
        DeleteSubscription(subscription_admin_client,
                           {project_id, subscription_id});
      });

  // Verify kAlreadyExists is returned.
  std::cout << "\nRunning CreateSubscription() [2] sample" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, topic_id, subscription_id});

  auto const table_id = project_id + ":samples.pubsub-subscription";
  std::cout << "\nRunning CreateBigQuerySubscription() sample" << std::endl;
  CreateBigQuerySubscription(
      subscription_admin_client,
      {project_id, topic_id, bigquery_subscription_id, table_id});
  cleanup.Defer([subscription_admin_client, project_id,
                 bigquery_subscription_id]() mutable {
    std::cout << "\nDelete subscription (" << bigquery_subscription_id << ")"
              << std::endl;
    subscription_admin_client.DeleteSubscription(
        pubsub::Subscription(project_id, bigquery_subscription_id).FullName());
  });

  auto const bucket_id = project_id + "-pubsub-bucket";
  std::cout << "\nRunning CreateCloudStorageSubscription() sample" << std::endl;
  CreateCloudStorageSubscription(
      subscription_admin_client,
      {project_id, topic_id, cloud_storage_subscription_id, bucket_id});
  cleanup.Defer([subscription_admin_client, project_id,
                 cloud_storage_subscription_id]() mutable {
    std::cout << "\nDelete subscription (" << cloud_storage_subscription_id
              << ")" << std::endl;
    subscription_admin_client.DeleteSubscription(
        pubsub::Subscription(project_id, cloud_storage_subscription_id)
            .FullName());
  });

  // Hardcode this number as it does not really matter. The other samples
  // pick something between 10 and 15.
  auto constexpr kDeadLetterDeliveryAttempts = 15;

  std::cout << "\nRunning CreateDeadLetterSubscription() sample" << std::endl;
  CreateDeadLetterSubscription(
      subscription_admin_client,
      {project_id, topic_id, dead_letter_subscription_id, dead_letter_topic_id,
       std::to_string(kDeadLetterDeliveryAttempts)});
  cleanup.Defer([subscription_admin_client, topic_admin_client, project_id,
                 dead_letter_topic, dead_letter_subscription_id]() mutable {
    // You must delete the subscription before the topic.
    std::cout << "\nDelete subscription (" << dead_letter_subscription_id << ")"
              << std::endl;
    subscription_admin_client.DeleteSubscription(
        pubsub::Subscription(project_id, dead_letter_subscription_id)
            .FullName());
    std::cout << "\nDelete topic (" << dead_letter_topic.topic_id() << ")"
              << std::endl;
    (void)topic_admin_client.DeleteTopic(dead_letter_topic.FullName());
  });

  auto constexpr kUpdatedDeadLetterDeliveryAttempts = 20;

  std::cout << "\nRunning UpdateDeadLetterSubscription() sample" << std::endl;
  ignore_emulator_failures([&] {
    UpdateDeadLetterSubscription(
        subscription_admin_client,
        {project_id, dead_letter_subscription_id, dead_letter_topic_id,
         std::to_string(kUpdatedDeadLetterDeliveryAttempts)});
  });

  std::cout << "\nRunning RemoveDeadLetterPolicy() sample" << std::endl;
  ignore_emulator_failures([&] {
    RemoveDeadLetterPolicy(subscription_admin_client,
                           {project_id, dead_letter_subscription_id});
  });

  std::cout
      << "\nRunning CreateSubscriptionWithExactlyOnceDelivery() sample [1]"
      << std::endl;
  CreateSubscriptionWithExactlyOnceDelivery(
      subscription_admin_client,
      {project_id, topic_id, exactly_once_subscription_id});
  cleanup.Defer([subscription_admin_client, project_id,
                 exactly_once_subscription_id]() mutable {
    std::cout << "\nDelete subscription (" << exactly_once_subscription_id
              << ")" << std::endl;
    subscription_admin_client.DeleteSubscription(
        pubsub::Subscription(project_id, exactly_once_subscription_id)
            .FullName());
  });

  std::cout
      << "\nRunning CreateSubscriptionWithExactlyOnceDelivery() sample [2]"
      << std::endl;
  CreateSubscriptionWithExactlyOnceDelivery(
      subscription_admin_client,
      {project_id, topic_id, exactly_once_subscription_id});

  std::cout << "\nRunning CreateFilteredSubscription() sample [1]" << std::endl;
  CreateFilteredSubscription(subscription_admin_client,
                             {project_id, topic_id, filtered_subscription_id});
  cleanup.Defer([subscription_admin_client, project_id,
                 filtered_subscription_id]() mutable {
    std::cout << "\nDelete subscription (" << filtered_subscription_id << ")"
              << std::endl;
    subscription_admin_client.DeleteSubscription(
        google::cloud::pubsub::Subscription(project_id,
                                            filtered_subscription_id)
            .FullName());
  });

  std::cout << "\nRunning CreateFilteredSubscription() sample [2]" << std::endl;
  CreateFilteredSubscription(subscription_admin_client,
                             {project_id, topic_id, filtered_subscription_id});
  std::cout << "\nRunning CreateOrderingSubscription() sample" << std::endl;
  CreateOrderingSubscription(
      subscription_admin_client,
      {project_id, ordering_topic_id, ordering_subscription_id});
  cleanup.Defer([subscription_admin_client, project_id,
                 ordering_subscription_id]() mutable {
    std::cout << "\nDelete subscription (" << ordering_subscription_id << ")"
              << std::endl;
    subscription_admin_client.DeleteSubscription(
        pubsub::Subscription(project_id, ordering_subscription_id).FullName());
  });
  auto const endpoint1 = "https://" + project_id + ".appspot.com/push1";
  auto const endpoint2 = "https://" + project_id + ".appspot.com/push2";
  std::cout << "\nRunning CreatePushSubscription() sample [1]" << std::endl;
  CreatePushSubscription(
      subscription_admin_client,
      {project_id, topic_id, push_subscription_id, endpoint1});
  cleanup.Defer(
      [subscription_admin_client, project_id, push_subscription_id]() mutable {
        std::cout << "\nDelete subscription (" << push_subscription_id << ")"
                  << std::endl;
        subscription_admin_client.DeleteSubscription(
            pubsub::Subscription(project_id, push_subscription_id).FullName());
      });

  std::cout << "\nRunning CreatePushSubscription() sample [2]" << std::endl;
  CreatePushSubscription(
      subscription_admin_client,
      {project_id, topic_id, push_subscription_id, endpoint1});

  std::cout << "\nRunning ModifyPushConfig() sample" << std::endl;
  ModifyPushConfig(subscription_admin_client,
                   {project_id, push_subscription_id, endpoint2});

  std::cout << "\nRunning CreateUnwrappedPushSubscription() sample [3]"
            << std::endl;
  CreateUnwrappedPushSubscription(
      subscription_admin_client,
      {project_id, topic_id, unwrapped_push_subscription_id, endpoint1});
  cleanup.Defer([subscription_admin_client, project_id,
                 unwrapped_push_subscription_id]() mutable {
    std::cout << "\nDelete subscription (" << unwrapped_push_subscription_id
              << ")" << std::endl;
    subscription_admin_client.DeleteSubscription(
        pubsub::Subscription(project_id, unwrapped_push_subscription_id)
            .FullName());
  });

  std::cout << "\nRunning CreateSnapshot() sample [1]" << std::endl;
  CreateSnapshot(subscription_admin_client,
                 {project_id, subscription_id, snapshot_id});
  cleanup.Defer([subscription_admin_client, project_id, snapshot_id]() mutable {
    std::cout << "\nDelete snapshot (" << snapshot_id << ")" << std::endl;
    subscription_admin_client.DeleteSnapshot(
        pubsub::Snapshot(project_id, snapshot_id).FullName());
  });

  std::cout << "\nRunning CreateSnapshot() sample [2]" << std::endl;
  CreateSnapshot(subscription_admin_client,
                 {project_id, subscription_id, snapshot_id});

  std::cout << "\nRunning GetSnapshot() sample" << std::endl;
  GetSnapshot(subscription_admin_client, {project_id, snapshot_id});

  std::cout << "\nRunning UpdateSnapshot() sample" << std::endl;
  ignore_emulator_failures([&] {
    UpdateSnapshot(subscription_admin_client, {project_id, snapshot_id});
  });

  std::cout << "\nRunning ListSnapshots() sample" << std::endl;
  ListSnapshots(subscription_admin_client, {project_id});

  std::cout << "\nRunning SeekWithSnapshot() sample" << std::endl;
  SeekWithSnapshot(subscription_admin_client,
                   {project_id, subscription_id, snapshot_id});

  std::cout << "\nRunning DeleteSnapshot() sample [1]" << std::endl;
  DeleteSnapshot(subscription_admin_client, {project_id, snapshot_id});

  std::cout << "\nRunning DeleteSnapshot() sample [2]" << std::endl;
  DeleteSnapshot(subscription_admin_client, {project_id, snapshot_id});

  std::cout << "\nRunning SeekWithTimestamp() sample" << std::endl;
  SeekWithTimestamp(subscription_admin_client,
                    {project_id, subscription_id, "2"});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateSubscriptionAdminCommand(
          "create-subscription", {"project-id", "topic-id", "subscription-id"},
          CreateSubscription),
      CreateSubscriptionAdminCommand(
          "create-bigquery-subscription",
          {"project-id", "topic-id", "subscription-id", "table-id"},
          CreateBigQuerySubscription),
      CreateSubscriptionAdminCommand(
          "create-cloud-storage-subscription",
          {"project-id", "topic-id", "subscription-id", "bucket"},
          CreateCloudStorageSubscription),
      CreateSubscriptionAdminCommand(
          "create-dead-letter-subscription",
          {"project-id", "topic-id", "subscription-id", "dead-letter-topic-id",
           "dead-letter-delivery-attempts"},
          CreateDeadLetterSubscription),
      CreateSubscriptionAdminCommand(
          "create-subscription-with-exactly-once-delivery",
          {"project-id", "topic-id", "subscription-id"},
          CreateSubscriptionWithExactlyOnceDelivery),
      CreateSubscriptionAdminCommand(
          "create-filtered-subscription",
          {"project-id", "topic-id", "subscription-id"},
          CreateFilteredSubscription),
      CreateSubscriptionAdminCommand(
          "create-ordering-subscription",
          {"project-id", "topic-id", "subscription-id"},
          CreateOrderingSubscription),
      CreateSubscriptionAdminCommand(
          "create-push-subscription",
          {"project-id", "topic-id", "subscription-id", "endpoint"},
          CreatePushSubscription),
      CreateSubscriptionAdminCommand(
          "create-unwrapped-push-subscription",
          {"project-id", "topic-id", "subscription-id", "endpoint"},
          CreateUnwrappedPushSubscription),
      CreateSubscriptionAdminCommand("remove-dead-letter-policy",
                                     {"project-id", "subscription-id"},
                                     RemoveDeadLetterPolicy),
      CreateSubscriptionAdminCommand(
          "update-dead-letter-subscription",
          {"project-id", "subscription-id", "dead-letter-topic-id",
           "dead-letter-delivery-attempts"},
          UpdateDeadLetterSubscription),
      CreateSubscriptionAdminCommand("get-subscription",
                                     {"project-id", "subscription-id"},
                                     GetSubscription),
      CreateSubscriptionAdminCommand("update-subscription",
                                     {"project-id", "subscription-id"},
                                     UpdateSubscription),
      CreateSubscriptionAdminCommand("list-subscriptions", {"project-id"},
                                     ListSubscriptions),
      CreateSubscriptionAdminCommand(
          "modify-push-config", {"project-id", "subscription-id", "endpoint"},
          ModifyPushConfig),
      CreateSubscriptionAdminCommand(
          "create-snapshot", {"project-id", "subscription-id", "snapshot-id"},
          CreateSnapshot),
      CreateSubscriptionAdminCommand(
          "get-snapshot", {"project-id", "snapshot-id"}, GetSnapshot),
      CreateSubscriptionAdminCommand(
          "update-snapshot", {"project-id", "snapshot-id"}, UpdateSnapshot),
      CreateSubscriptionAdminCommand("list-snapshots", {"project-id"},
                                     ListSnapshots),
      CreateSubscriptionAdminCommand(
          "delete-snapshot", {"project-id", "snapshot-id"}, DeleteSnapshot),
      CreateSubscriptionAdminCommand(
          "seek-with-snapshot",
          {"project-id", "subscription-id", "snapshot-id"}, SeekWithSnapshot),
      CreateSubscriptionAdminCommand(
          "seek-with-timestamp", {"project-id", "subscription-id", "seconds"},
          SeekWithTimestamp),
      CreateSubscriptionAdminCommand("delete-subscription",
                                     {"project-id", "subscription-id"},
                                     DeleteSubscription),
      {"auto", AutoRun},
  });

  return example.Run(argc, argv);
}
