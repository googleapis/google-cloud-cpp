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

#include "google/cloud/pubsub/samples/pubsub_samples_common.h"
#include "google/cloud/pubsub/schema_admin_client.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/subscription_builder.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <google/cloud/pubsub/samples/samples.pb.h>
#include <condition_variable>
#include <mutex>
#include <tuple>
#include <utility>

namespace {
std::string RandomTopicId(google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomTopicId(generator,
                                                      "cloud-cpp-samples");
}

std::string RandomSubscriptionId(
    google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomSubscriptionId(
      generator, "cloud-cpp-samples");
}

std::string RandomSnapshotId(google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomSnapshotId(generator,
                                                         "cloud-cpp-samples");
}

std::string RandomSchemaId(google::cloud::internal::DefaultPRNG& generator) {
  return google::cloud::pubsub_testing::RandomSchemaId(generator,
                                                       "cloud-cpp-samples");
}

auto constexpr kWaitTimeout = std::chrono::minutes(1);

void WaitForSession(google::cloud::future<google::cloud::Status> session,
                    std::string const& name) {
  std::cout << "\nWaiting for session [" << name << "]... " << std::flush;
  auto result = session.wait_for(kWaitTimeout);
  if (result == std::future_status::timeout) {
    std::cout << "TIMEOUT" << std::endl;
    throw std::runtime_error("session timeout");
  }
  std::cout << "DONE (" << session.get() << ")" << std::endl;
}

/// A (file) singleton to track received messages.
class EventCounter {
 public:
  EventCounter() = default;

  void Increment() {
    std::lock_guard<std::mutex> lk(mu_);
    ++counter_;
    cv_.notify_all();
  }

  std::int64_t Current() {
    std::lock_guard<std::mutex> lk(mu_);
    return counter_;
  }

  template <typename Predicate>
  void Wait(Predicate&& predicate,
            google::cloud::future<google::cloud::Status> session,
            std::string const& name) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, kWaitTimeout,
                 [this, &predicate] { return predicate(counter_); });
    lk.unlock();
    session.cancel();
    WaitForSession(std::move(session), name);
  }

  template <typename Predicate>
  void Wait(Predicate&& predicate) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, kWaitTimeout,
                 [this, &predicate] { return predicate(counter_); });
  }

  static EventCounter& Instance() {
    static auto* const kInstance = new EventCounter;
    return *kInstance;
  }

 private:
  std::int64_t counter_ = 0;
  std::mutex mu_;
  std::condition_variable cv_;
};

/**
 * Count received messages to gracefully shutdown the samples.
 *
 * This function is used in the examples to simplify their shutdown and testing.
 * Most applications probably do not need to worry about gracefully shutting
 * down a subscriber. However, the examples are tested as part of our CI process
 * and do need to shutdown gracefully. To do so, we count the number of events
 * received in a subscriber, and once enough messages (the count depends on the
 * example) are received we cancel the session (the object returned by
 * `pubsub::Subscriber::Subscribe()`) and wait for the session to shutdown.
 *
 * This function is named as a hint to the readers, so they can ignore when
 * understanding the sample code.
 */
void PleaseIgnoreThisSimplifiesTestingTheSamples() {
  EventCounter::Instance().Increment();
}

void CreateTopic(google::cloud::pubsub::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [START pubsub_quickstart_create_topic]
  //! [START pubsub_create_topic] [create-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.CreateTopic(pubsub::TopicBuilder(
        pubsub::Topic(std::move(project_id), std::move(topic_id))));
    // Note that kAlreadyExists is a possible error when the library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::runtime_error(topic.status().message());

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  //! [END pubsub_create_topic] [create-topic]
  //! [END pubsub_quickstart_create_topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetTopic(google::cloud::pubsub::TopicAdminClient client,
              std::vector<std::string> const& argv) {
  //! [get-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.GetTopic(
        pubsub::Topic(std::move(project_id), std::move(topic_id)));
    if (!topic) throw std::runtime_error(topic.status().message());

    std::cout << "The topic information was successfully retrieved: "
              << topic->DebugString() << "\n";
  }
  //! [get-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateTopic(google::cloud::pubsub::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [update-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.UpdateTopic(
        pubsub::TopicBuilder(
            pubsub::Topic(std::move(project_id), std::move(topic_id)))
            .add_label("test-key", "test-value"));
    if (!topic) return;  // TODO(#4792) - emulator lacks UpdateTopic()

    std::cout << "The topic was successfully updated: " << topic->DebugString()
              << "\n";
  }
  //! [update-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopics(google::cloud::pubsub::TopicAdminClient client,
                std::vector<std::string> const& argv) {
  //! [START pubsub_list_topics] [list-topics]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string const& project_id) {
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
  //! [END pubsub_list_topics] [list-topics]
  (std::move(client), argv.at(0));
}

void DeleteTopic(google::cloud::pubsub::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [START pubsub_delete_topic] [delete-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string const& project_id,
     std::string const& topic_id) {
    auto status = client.DeleteTopic(
        pubsub::Topic(std::move(project_id), std::move(topic_id)));
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The topic was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The topic was successfully deleted\n";
  }
  //! [END pubsub_delete_topic] [delete-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void DetachSubscription(google::cloud::pubsub::TopicAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [START pubsub_detach_subscription] [detach-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string subscription_id) {
    auto response = client.DetachSubscription(pubsub::Subscription(
        std::move(project_id), std::move(subscription_id)));
    if (!response.ok()) return;  // TODO(#4792) - not implemented in emulator

    std::cout << "The subscription was successfully detached: "
              << response->DebugString() << "\n";
  }
  //! [END pubsub_detach_subscription] [detach-subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopicSubscriptions(google::cloud::pubsub::TopicAdminClient client,
                            std::vector<std::string> const& argv) {
  //! [START pubsub_list_topic_subscriptions] [list-topic-subscriptions]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto const topic =
        pubsub::Topic(std::move(project_id), std::move(topic_id));
    std::cout << "Subscription list for topic " << topic << ":\n";
    for (auto const& name : client.ListTopicSubscriptions(topic)) {
      if (!name) throw std::runtime_error(name.status().message());
      std::cout << "  " << *name << "\n";
    }
  }
  //! [END pubsub_list_topic_subscriptions] [list-topic-subscriptions]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListTopicSnapshots(google::cloud::pubsub::TopicAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [list-topic-snapshots]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto const topic =
        pubsub::Topic(std::move(project_id), std::move(topic_id));
    std::cout << "Snapshot list for topic " << topic << ":\n";
    for (auto const& name : client.ListTopicSnapshots(topic)) {
      if (!name) throw std::runtime_error(name.status().message());
      std::cout << "  " << *name << "\n";
    }
  }
  //! [list-topic-snapshots]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateSubscription(google::cloud::pubsub::SubscriptionAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [START pubsub_create_pull_subscription] [create-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& topic_id, std::string const& subscription_id) {
    auto sub = client.CreateSubscription(
        pubsub::Topic(project_id, std::move(topic_id)),
        pubsub::Subscription(project_id, std::move(subscription_id)));
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  //! [END pubsub_create_pull_subscription] [create-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateFilteredSubscription(
    google::cloud::pubsub::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  //! [create-filtered-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string topic_id, std::string subscription_id) {
    auto sub = client.CreateSubscription(
        pubsub::Topic(project_id, std::move(topic_id)),
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.set_filter(
            R"""(attributes.is-even = "false")"""));
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  //! [create-filtered-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreatePushSubscription(
    google::cloud::pubsub::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  //! [START pubsub_create_push_subscription] [create-push-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& topic_id, std::string const& subscription_id,
     std::string const& endpoint) {
    auto sub = client.CreateSubscription(
        pubsub::Topic(project_id, std::move(topic_id)),
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.set_push_config(
            pubsub::PushConfigBuilder{}.set_push_endpoint(endpoint)));
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  //! [END pubsub_create_push_subscription] [create-push-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateOrderingSubscription(
    google::cloud::pubsub::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  //! [START pubsub_enable_subscription_ordering] [enable-subscription-ordering]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& topic_id, std::string const& subscription_id) {
    auto sub = client.CreateSubscription(
        pubsub::Topic(project_id, std::move(topic_id)),
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.enable_message_ordering(true));
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  //! [END pubsub_enable_subscription_ordering] [enable-subscription-ordering]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateDeadLetterSubscription(
    google::cloud::pubsub::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  //! [dead-letter-create-subscription]
  // [START pubsub_dead_letter_create_subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& topic_id, std::string const& subscription_id,
     std::string const& dead_letter_topic_id,
     int dead_letter_delivery_attempts) {
    auto sub = client.CreateSubscription(
        pubsub::Topic(project_id, std::move(topic_id)),
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.set_dead_letter_policy(
            pubsub::SubscriptionBuilder::MakeDeadLetterPolicy(
                pubsub::Topic(project_id, dead_letter_topic_id),
                dead_letter_delivery_attempts)));
    if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The subscription already exists\n";
      return;
    }
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";

    std::cout << "It will forward dead letter messages to: "
              << sub->dead_letter_policy().dead_letter_topic() << "\n";

    std::cout << "After " << sub->dead_letter_policy().max_delivery_attempts()
              << " delivery attempts.\n";
  }
  // [END pubsub_dead_letter_create_subscription]
  //! [dead-letter-create-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3),
   std::stoi(argv.at(4)));
}

void UpdateDeadLetterSubscription(
    google::cloud::pubsub::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  //! [dead-letter-update-subscription]
  // [START pubsub_dead_letter_update_subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id,
     std::string const& dead_letter_topic_id,
     int dead_letter_delivery_attempts) {
    auto sub = client.UpdateSubscription(
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.set_dead_letter_policy(
            pubsub::SubscriptionBuilder::MakeDeadLetterPolicy(
                pubsub::Topic(project_id, dead_letter_topic_id),
                dead_letter_delivery_attempts)));
    if (!sub) return;  // TODO(#4792) - emulator lacks UpdateSubscription()

    std::cout << "The subscription has been updated to: " << sub->DebugString()
              << "\n";

    std::cout << "It will forward dead letter messages to: "
              << sub->dead_letter_policy().dead_letter_topic() << "\n";

    std::cout << "After " << sub->dead_letter_policy().max_delivery_attempts()
              << " delivery attempts.\n";
  }
  // [END pubsub_dead_letter_update_subscription]
  //! [dead-letter-update-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2),
   std::stoi(argv.at(3)));
}

void ReceiveDeadLetterDeliveryAttempt(
    google::cloud::pubsub::Subscriber subscriber,
    std::vector<std::string> const&) {
  auto const initial = EventCounter::Instance().Current();
  //! [dead-letter-delivery-attempt]
  // [START pubsub_dead_letter_delivery_attempt]
  namespace pubsub = google::cloud::pubsub;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          std::cout << "Delivery attempt: " << h.delivery_attempt() << "\n";
          std::move(h).ack();
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
  };
  // [END pubsub_dead_letter_delivery_attempt]
  //! [dead-letter-delivery-attempt]
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) { return count > initial; },
      sample(std::move(subscriber)), __func__);
}

void RemoveDeadLetterPolicy(
    google::cloud::pubsub::SubscriptionAdminClient client,
    std::vector<std::string> const& argv) {
  //! [START pubsub_dead_letter_remove] [dead-letter-remove]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id) {
    auto sub = client.UpdateSubscription(
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.clear_dead_letter_policy());
    if (!sub) return;  // TODO(#4792) - emulator lacks UpdateSubscription()

    std::cout << "The subscription has been updated to: " << sub->DebugString()
              << "\n";
  }
  //! [END pubsub_dead_letter_remove] [dead-letter-remove]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetSubscription(google::cloud::pubsub::SubscriptionAdminClient client,
                     std::vector<std::string> const& argv) {
  //! [get-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id) {
    auto sub = client.GetSubscription(
        pubsub::Subscription(project_id, std::move(subscription_id)));
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription exists and its metadata is: "
              << sub->DebugString() << "\n";
  }
  //! [get-subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateSubscription(google::cloud::pubsub::SubscriptionAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [update-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id) {
    auto s = client.UpdateSubscription(
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::SubscriptionBuilder{}.set_ack_deadline(
            std::chrono::seconds(60)));
    if (!s) throw std::runtime_error(s.status().message());

    std::cout << "The subscription has been updated to: " << s->DebugString()
              << "\n";
  }
  //! [update-subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListSubscriptions(google::cloud::pubsub::SubscriptionAdminClient client,
                       std::vector<std::string> const& argv) {
  //! [START pubsub_list_subscriptions] [list-subscriptions]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id) {
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
  //! [END pubsub_list_subscriptions] [list-subscriptions]
  (std::move(client), argv.at(0));
}

void DeleteSubscription(google::cloud::pubsub::SubscriptionAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [START pubsub_delete_subscription] [delete-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id) {
    auto status = client.DeleteSubscription(pubsub::Subscription(
        std::move(project_id), std::move(subscription_id)));
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The subscription was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The subscription was successfully deleted\n";
  }
  //! [END pubsub_delete_subscription] [delete-subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void ModifyPushConfig(google::cloud::pubsub::SubscriptionAdminClient client,
                      std::vector<std::string> const& argv) {
  //! [START pubsub_update_push_configuration] [modify-push-config]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id, std::string const& endpoint) {
    auto status = client.ModifyPushSubscription(
        pubsub::Subscription(project_id, std::move(subscription_id)),
        pubsub::PushConfigBuilder{}.set_push_endpoint(endpoint));
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The subscription push configuration was successfully"
              << " modified\n";
  }
  //! [END pubsub_update_push_configuration] [modify-push-config]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateSnapshot(google::cloud::pubsub::SubscriptionAdminClient client,
                    std::vector<std::string> const& argv) {
  //! [create-snapshot-with-name]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id, std::string const& snapshot_id) {
    auto snapshot = client.CreateSnapshot(
        pubsub::Subscription(project_id, subscription_id),
        pubsub::Snapshot(project_id, std::move(snapshot_id)));
    if (snapshot.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The snapshot already exists\n";
      return;
    }
    if (!snapshot.ok()) throw std::runtime_error(snapshot.status().message());

    std::cout << "The snapshot was successfully created: "
              << snapshot->DebugString() << "\n";
  }
  //! [create-snapshot-with-name]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetSnapshot(google::cloud::pubsub::SubscriptionAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [get-snapshot]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& snapshot_id) {
    auto response = client.GetSnapshot(
        pubsub::Snapshot(std::move(project_id), std::move(snapshot_id)));
    if (!response.ok()) throw std::runtime_error(response.status().message());

    std::cout << "The snapshot details are: " << response->DebugString()
              << "\n";
  }
  //! [get-snapshot]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateSnapshot(google::cloud::pubsub::SubscriptionAdminClient client,
                    std::vector<std::string> const& argv) {
  //! [update-snapshot]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string snapshot_id) {
    auto snap = client.UpdateSnapshot(
        pubsub::Snapshot(project_id, std::move(snapshot_id)),
        pubsub::SnapshotBuilder{}.add_label("samples-cpp", "gcp"));
    if (!snap.ok()) return;  // TODO(#4792) - emulator lacks UpdateSnapshot()

    std::cout << "The snapshot was successfully updated: "
              << snap->DebugString() << "\n";
  }
  //! [update-snapshot]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListSnapshots(google::cloud::pubsub::SubscriptionAdminClient client,
                   std::vector<std::string> const& argv) {
  //! [list-snapshots]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id) {
    std::cout << "Snapshot list for project " << project_id << ":\n";
    for (auto const& snapshot : client.ListSnapshots(project_id)) {
      if (!snapshot) throw std::runtime_error(snapshot.status().message());
      std::cout << "Snapshot Name: " << snapshot->name() << "\n";
    }
  }
  //! [list-snapshots]
  (std::move(client), argv.at(0));
}

void DeleteSnapshot(google::cloud::pubsub::SubscriptionAdminClient client,
                    std::vector<std::string> const& argv) {
  //! [delete-snapshot]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& snapshot_id) {
    auto status = client.DeleteSnapshot(
        pubsub::Snapshot(std::move(project_id), std::move(snapshot_id)));
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The snapshot was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The snapshot was successfully deleted\n";
  }
  //! [delete-snapshot]
  (std::move(client), argv.at(0), argv.at(1));
}

void SeekWithSnapshot(google::cloud::pubsub::SubscriptionAdminClient client,
                      std::vector<std::string> const& argv) {
  //! [seek-with-snapshot]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id, std::string const& snapshot_id) {
    auto response =
        client.Seek(pubsub::Subscription(project_id, subscription_id),
                    pubsub::Snapshot(project_id, snapshot_id));
    if (!response.ok()) throw std::runtime_error(response.status().message());

    std::cout << "The subscription seek was successful: "
              << response->DebugString() << "\n";
  }
  //! [seek-with-snapshot]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void SeekWithTimestamp(google::cloud::pubsub::SubscriptionAdminClient client,
                       std::vector<std::string> const& argv) {
  //! [seek-with-timestamp]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id, std::string const& seconds) {
    auto response =
        client.Seek(pubsub::Subscription(project_id, subscription_id),
                    std::chrono::system_clock::now() -
                        std::chrono::seconds(std::stoi(seconds)));
    if (!response.ok()) throw std::runtime_error(response.status().message());

    std::cout << "The subscription seek was successful: "
              << response->DebugString() << "\n";
  }
  //! [seek-with-timestamp]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void ExampleStatusOr(google::cloud::pubsub::TopicAdminClient client,
                     std::vector<std::string> const& argv) {
  //! [example-status-or]
  namespace pubsub = ::google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string const& project_id) {
    // The actual type of `topic` is
    // google::cloud::StatusOr<google::pubsub::v1::Topic>, but
    // we expect it'll most often be declared with auto like this.
    for (auto const& topic : client.ListTopics(project_id)) {
      // Use `topic` like a smart pointer; check it before de-referencing
      if (!topic) {
        // `topic` doesn't contain a value, so `.status()` will contain error
        // info
        std::cerr << topic.status() << "\n";
        break;
      }
      std::cout << topic->DebugString() << "\n";
    }
  }
  //! [example-status-or]
  (std::move(client), argv.at(0));
}

void CreateAvroSchema(google::cloud::pubsub::SchemaAdminClient client,
                      std::vector<std::string> const& argv) {
  //! [START pubsub_create_avro_schema] [create-avro-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id,
     std::string const& schema_id) {
    auto constexpr kDefinition = R"js({
      "type": "record",
      "name": "State",
      "namespace": "utilities",
      "doc": "A list of states in the United States of America.",
      "fields": [
        {
          "name": "name",
          "type": "string",
          "doc": "The common name of the state."
        },
        {
          "name": "post_abbr",
          "type": "string",
          "doc": "The postal code abbreviation of the state."
        }
      ]
    })js";
    auto schema = client.CreateAvroSchema(pubsub::Schema(project_id, schema_id),
                                          kDefinition);
    if (schema.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The schema already exists\n";
      return;
    }
    if (!schema) throw std::runtime_error(schema.status().message());

    std::cout << "Schema successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [END pubsub_create_avro_schema] [create-avro-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateProtobufSchema(google::cloud::pubsub::SchemaAdminClient client,
                          std::vector<std::string> const& argv) {
  //! [START pubsub_create_proto_schema] [create-protobuf-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id,
     std::string const& schema_id) {
    auto constexpr kDefinition = R"pfile(
        syntax = "proto3";
        package google.cloud.pubsub.samples;

        message State {
          string name = 1;
          string post_abbr = 2;
        }
        )pfile";
    auto schema = client.CreateProtobufSchema(
        pubsub::Schema(project_id, schema_id), kDefinition);
    if (schema.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The schema already exists\n";
      return;
    }
    if (!schema) return;  // TODO(#4792) - protobuf schema support in emulator
    std::cout << "Schema successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [END pubsub_create_proto_schema] [create-protobuf-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetSchema(google::cloud::pubsub::SchemaAdminClient client,
               std::vector<std::string> const& argv) {
  //! [START pubsub_get_schema] [get-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id,
     std::string const& schema_id) {
    auto schema = client.GetSchema(pubsub::Schema(project_id, schema_id),
                                   google::pubsub::v1::FULL);
    if (!schema) throw std::runtime_error(schema.status().message());

    std::cout << "The schema exists and its metadata is: "
              << schema->DebugString() << "\n";
  }
  //! [END pubsub_get_schema] [get-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListSchemas(google::cloud::pubsub::SchemaAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [START pubsub_list_schemas] [list-schemas]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id) {
    for (auto const& schema :
         client.ListSchemas(project_id, google::pubsub::v1::FULL)) {
      if (!schema) throw std::runtime_error(schema.status().message());
      std::cout << "Schema: " << schema->DebugString() << "\n";
    }
  }
  //! [END pubsub_list_schemas] [list-schemas]
  (std::move(client), argv.at(0));
}

void DeleteSchema(google::cloud::pubsub::SchemaAdminClient client,
                  std::vector<std::string> const& argv) {
  //! [START pubsub_delete_schema] [delete-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id,
     std::string const& schema_id) {
    auto status = client.DeleteSchema(pubsub::Schema(project_id, schema_id));
    // Note that kNotFound is a possible result when the library retries.
    if (status.code() == google::cloud::StatusCode::kNotFound) {
      std::cout << "The schema was not found\n";
      return;
    }
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "Schema successfully deleted\n";
  }
  //! [END pubsub_delete_schema] [delete-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void ValidateAvroSchema(google::cloud::pubsub::SchemaAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [validate-avro-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id) {
    auto constexpr kDefinition = R"js({
      "type": "record",
      "name": "State",
      "namespace": "utilities",
      "doc": "A list of states in the United States of America.",
      "fields": [
        {
          "name": "name",
          "type": "string",
          "doc": "The common name of the state."
        },
        {
          "name": "post_abbr",
          "type": "string",
          "doc": "The postal code abbreviation of the state."
        }
      ]
    })js";
    auto schema = client.ValidateAvroSchema(project_id, kDefinition);
    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema is valid\n";
  }
  //! [validate-avro-schema]
  (std::move(client), argv.at(0));
}

void ValidateProtobufSchema(google::cloud::pubsub::SchemaAdminClient client,
                            std::vector<std::string> const& argv) {
  //! [validate-protobuf-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id) {
    auto constexpr kDefinition = R"pfile(
        syntax = "proto3";
        package google.cloud.pubsub.samples;

        message State {
          string name = 1;
          string post_abbr = 2;
        }
        )pfile";
    auto schema = client.ValidateProtobufSchema(project_id, kDefinition);
    if (!schema) return;  // TODO(#4792) - protobuf schema support in emulator
    std::cout << "Schema is valid\n";
  }
  //! [validate-protobuf-schema]
  (std::move(client), argv.at(0));
}

void ValidateMessageAvro(google::cloud::pubsub::SchemaAdminClient client,
                         std::vector<std::string> const& argv) {
  //! [validate-message-avro]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id) {
    auto constexpr kDefinition = R"js({
      "type": "record",
      "name": "State",
      "namespace": "google.cloud.pubsub.samples",
      "doc": "A list of states in the United States of America.",
      "fields": [
        {
          "name": "name",
          "type": "string",
          "doc": "The common name of the state."
        },
        {
          "name": "post_abbr",
          "type": "string",
          "doc": "The postal code abbreviation of the state."
        }
      ]
    })js";
    auto constexpr kMessage = R"js({
          "name": "New York",
          "post_abbr": "NY"
    })js";
    auto schema = client.ValidateMessageWithAvro(
        google::pubsub::v1::JSON, kMessage, project_id, kDefinition);
    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Message is valid\n";
  }
  //! [validate-message-avro]
  (std::move(client), argv.at(0));
}

void ValidateMessageProtobuf(google::cloud::pubsub::SchemaAdminClient client,
                             std::vector<std::string> const& argv) {
  //! [validate-message-protobuf]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id) {
    google::cloud::pubsub::samples::State data;
    data.set_name("New York");
    data.set_post_abbr("NY");
    auto const message = data.SerializeAsString();
    auto constexpr kDefinition = R"pfile(
        syntax = "proto3";
        package google.cloud.pubsub.samples;

        message State {
          string name = 1;
          string post_abbr = 2;
        }
    )pfile";
    auto schema = client.ValidateMessageWithProtobuf(
        google::pubsub::v1::BINARY, message, project_id, kDefinition);
    if (!schema) return;  // TODO(#4792) - protobuf schema support in emulator
    std::cout << "Schema is valid\n";
  }
  //! [validate-message-protobuf]
  (std::move(client), argv.at(0));
}

void ValidateMessageNamedSchema(google::cloud::pubsub::SchemaAdminClient client,
                                std::vector<std::string> const& argv) {
  //! [validate-message-named-schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SchemaAdminClient client, std::string const& project_id,
     std::string const& schema_id) {
    google::cloud::pubsub::samples::State data;
    data.set_name("New York");
    data.set_post_abbr("NY");
    auto const message = data.SerializeAsString();
    auto schema = client.ValidateMessageWithNamedSchema(
        google::pubsub::v1::BINARY, message,
        pubsub::Schema(project_id, schema_id));
    if (!schema) return;  // TODO(#4792) - protobuf schema support in emulator
    std::cout << "Schema is valid\n";
  }
  //! [validate-message-named-schema]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateTopicWithSchema(google::cloud::pubsub::TopicAdminClient client,
                           std::vector<std::string> const& argv) {
  //! [START pubsub_create_topic_with_schema]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id, std::string schema_id, std::string const& encoding) {
    auto const& schema = pubsub::Schema(project_id, std::move(schema_id));
    auto topic = client.CreateTopic(
        pubsub::TopicBuilder(
            pubsub::Topic(std::move(project_id), std::move(topic_id)))
            .set_schema(schema)
            .set_encoding(encoding == "JSON" ? google::pubsub::v1::JSON
                                             : google::pubsub::v1::BINARY));
    // Note that kAlreadyExists is a possible error when the library retries.
    if (topic.status().code() == google::cloud::StatusCode::kAlreadyExists) {
      std::cout << "The topic already exists\n";
      return;
    }
    if (!topic) throw std::runtime_error(topic.status().message());

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  //! [END pubsub_create_topic_with_schema]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void PublishAvroRecords(google::cloud::pubsub::Publisher publisher,
                        std::vector<std::string> const&) {
  //! [START pubsub_publish_avro_records]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    auto constexpr kNewYork =
        R"js({ "name": "New York", "post_abbr": "NY" })js";
    auto constexpr kPennsylvania =
        R"js({ "name": "Pennsylvania", "post_abbr": "PA" })js";
    std::vector<future<void>> done;
    auto handler = [](future<StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::runtime_error(id.status().message());
    };
    for (auto const* data : {kNewYork, kPennsylvania}) {
      done.push_back(
          publisher.Publish(pubsub::MessageBuilder{}.SetData(data).Build())
              .then(handler));
    }
    // Block until all messages are published.
    for (auto& d : done) d.get();
  }
  //! [END pubsub_publish_avro_records]
  (std::move(publisher));
}

google::cloud::future<google::cloud::Status> SubscribeAvroRecords(
    google::cloud::pubsub::Subscriber subscriber,
    std::vector<std::string> const&) {
  //! [START pubsub_subscribe_avro_records]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  return [](pubsub::Subscriber subscriber) {
    auto session = subscriber.Subscribe(
        [](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Message contents: " << m.data() << "\n";
          std::move(h).ack();
        });
    return session;
  }
  //! [END pubsub_subscribe_avro_records]
  (std::move(subscriber));
}

void PublishProtobufRecords(google::cloud::pubsub::Publisher publisher,
                            std::vector<std::string> const&) {
  //! [START pubsub_publish_proto_messages]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    std::vector<std::pair<std::string, std::string>> states{
        {"New York", "NY"},
        {"Pennsylvania", "PA"},
    };
    std::vector<future<void>> done;
    auto handler = [](future<StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::runtime_error(id.status().message());
    };
    for (auto const& data : states) {
      google::cloud::pubsub::samples::State state;
      state.set_name(data.first);
      state.set_post_abbr(data.second);
      done.push_back(publisher
                         .Publish(pubsub::MessageBuilder{}
                                      .SetData(state.SerializeAsString())
                                      .Build())
                         .then(handler));
    }
    // Block until all messages are published.
    for (auto& d : done) d.get();
  }
  //! [END pubsub_publish_proto_messages]
  (std::move(publisher));
}

google::cloud::future<google::cloud::Status> SubscribeProtobufRecords(
    google::cloud::pubsub::Subscriber subscriber,
    std::vector<std::string> const&) {
  //! [START pubsub_subscribe_proto_messages]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  return [](pubsub::Subscriber subscriber) {
    auto session = subscriber.Subscribe(
        [](pubsub::Message const& m, pubsub::AckHandler h) {
          google::cloud::pubsub::samples::State state;
          state.ParseFromString(m.data());
          std::cout << "Message contents: " << state.DebugString() << "\n";
          std::move(h).ack();
        });
    return session;
  }
  //! [END pubsub_subscribe_proto_messages]
  (std::move(subscriber));
}

void Publish(google::cloud::pubsub::Publisher publisher,
             std::vector<std::string> const&) {
  //! [START pubsub_publish_messages_error_handler]
  //! [START pubsub_publish_with_error_handler]
  //! [START pubsub_publish] [publish]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    auto message_id = publisher.Publish(
        pubsub::MessageBuilder{}.SetData("Hello World!").Build());
    auto done = message_id.then([](future<StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::runtime_error(id.status().message());
      std::cout << "Hello World! published with id=" << *id << "\n";
    });
    // Block until the message is published
    done.get();
  }
  //! [END pubsub_publish] [publish]
  //! [END pubsub_publish_with_error_handler]
  //! [END pubsub_publish_messages_error_handler]
  (std::move(publisher));
}

void PublishCustomAttributes(google::cloud::pubsub::Publisher publisher,
                             std::vector<std::string> const&) {
  //! [START pubsub_publish_custom_attributes] [publish-custom-attributes]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    std::vector<future<void>> done;
    for (int i = 0; i != 10; ++i) {
      auto message_id = publisher.Publish(
          pubsub::MessageBuilder{}
              .SetData("Hello World! [" + std::to_string(i) + "]")
              .SetAttribute("origin", "cpp-sample")
              .SetAttribute("username", "gcp")
              .Build());
      done.push_back(message_id.then([i](future<StatusOr<std::string>> f) {
        auto id = f.get();
        if (!id) throw std::runtime_error(id.status().message());
        std::cout << "Message " << i << " published with id=" << *id << "\n";
      }));
    }
    publisher.Flush();
    // Block until all the messages are published (optional)
    for (auto& f : done) f.get();
  }
  //! [END pubsub_publish_custom_attributes] [publish-custom-attributes]
  (std::move(publisher));
}

// This is a helper function to publish N messages later consumed by another
// example.
void PublishHelper(google::cloud::pubsub::Publisher publisher,
                   std::string const& prefix, int message_count) {
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  std::vector<future<StatusOr<std::string>>> done;
  done.reserve(message_count);
  for (int i = 0; i != message_count; ++i) {
    std::string const value = i % 2 == 0 ? "true" : "false";
    done.push_back(
        publisher.Publish(pubsub::MessageBuilder{}
                              .SetAttribute("is-even", value)
                              .SetData(prefix + " [" + std::to_string(i) + "]")
                              .Build()));
  }
  publisher.Flush();
  for (auto& f : done) f.get();
}

void PublishOrderingKey(google::cloud::pubsub::Publisher publisher,
                        std::vector<std::string> const&) {
  //! [START pubsub_publish_with_ordering_keys] [publish-with-ordering-keys]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    struct SampleData {
      std::string ordering_key;
      std::string data;
    } data[] = {
        {"key1", "message1"}, {"key2", "message2"}, {"key1", "message3"},
        {"key1", "message4"}, {"key1", "message5"},
    };
    std::vector<future<void>> done;
    for (auto const& datum : data) {
      auto message_id =
          publisher.Publish(pubsub::MessageBuilder{}
                                .SetData("Hello World! [" + datum.data + "]")
                                .SetOrderingKey(datum.ordering_key)
                                .Build());
      std::string ack_id = datum.ordering_key + "#" + datum.data;
      done.push_back(message_id.then([ack_id](future<StatusOr<std::string>> f) {
        auto id = f.get();
        if (!id) throw std::runtime_error(id.status().message());
        std::cout << "Message " << ack_id << " published with id=" << *id
                  << "\n";
      }));
    }
    publisher.Flush();
    // Block until all the messages are published (optional)
    for (auto& f : done) f.get();
  }
  //! [END pubsub_publish_with_ordering_keys] [publish-with-ordering-keys]
  (std::move(publisher));
}

void ResumeOrderingKey(google::cloud::pubsub::Publisher publisher,
                       std::vector<std::string> const&) {
  //! [START pubsub_resume_publish_with_ordering_keys] [resume-publish]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    struct SampleData {
      std::string ordering_key;
      std::string data;
    } data[] = {
        {"key1", "message1"}, {"key2", "message2"}, {"key1", "message3"},
        {"key1", "message4"}, {"key1", "message5"},
    };
    std::vector<future<void>> done;
    for (auto const& datum : data) {
      auto const& da = datum;  // workaround MSVC lambda capture confusion
      auto handler = [da, publisher](future<StatusOr<std::string>> f) mutable {
        auto const msg = da.ordering_key + "#" + da.data;
        auto id = f.get();
        if (!id) {
          std::cout << "An error has occurred publishing " << msg << "\n";
          publisher.ResumePublish(da.ordering_key);
          return;
        }
        std::cout << "Message " << msg << " published as id=" << *id << "\n";
      };
      done.push_back(
          publisher
              .Publish(pubsub::MessageBuilder{}
                           .SetData("Hello World! [" + datum.data + "]")
                           .SetOrderingKey(datum.ordering_key)
                           .Build())
              .then(handler));
    }
    publisher.Flush();
    // Block until all the messages are published (optional)
    for (auto& f : done) f.get();
  }
  //! [END pubsub_resume_publish_with_ordering_keys] [resume-publish]
  (std::move(publisher));
}

void Subscribe(google::cloud::pubsub::Subscriber subscriber,
               std::vector<std::string> const&) {
  auto const initial = EventCounter::Instance().Current();
  //! [START pubsub_quickstart_subscriber]
  //! [START pubsub_subscriber_async_pull] [subscribe]
  namespace pubsub = google::cloud::pubsub;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          std::move(h).ack();
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
  };
  //! [END pubsub_subscriber_async_pull] [subscribe]
  //! [END pubsub_quickstart_subscriber]
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) { return count > initial; },
      sample(std::move(subscriber)), __func__);
}

void SubscribeErrorListener(google::cloud::pubsub::Subscriber subscriber,
                            std::vector<std::string> const&) {
  auto current = EventCounter::Instance().Current();
  // [START pubsub_subscriber_error_listener]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber
        .Subscribe([&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          std::move(h).ack();
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        })
        // Setup an error handler for the subscription session
        .then([](future<google::cloud::Status> f) {
          std::cout << "Subscription session result: " << f.get() << "\n";
        });
  };
  // [END pubsub_subscriber_error_listener]
  auto session = sample(std::move(subscriber));
  EventCounter::Instance().Wait(
      [current](std::int64_t count) { return count > current; });
  session.cancel();
  session.get();
}

void SubscribeCustomAttributes(google::cloud::pubsub::Subscriber subscriber,
                               std::vector<std::string> const&) {
  auto const initial = EventCounter::Instance().Current();
  //! [START pubsub_subscriber_async_pull_custom_attributes]
  namespace pubsub = google::cloud::pubsub;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message with attributes:\n";
          for (auto const& kv : m.attributes()) {
            std::cout << "  " << kv.first << ": " << kv.second << "\n";
          }
          std::move(h).ack();
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
  };
  //! [END pubsub_subscriber_async_pull_custom_attributes]
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) { return count > initial; },
      sample(std::move(subscriber)), __func__);
}

void CustomThreadPoolPublisher(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "custom-thread-pool-publisher <project-id> <topic-id>"};
  }
  //! [custom-thread-pool-publisher]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    // Create our own completion queue to run the background activity, such as
    // flushing the publisher.
    google::cloud::CompletionQueue cq;
    // Setup one or more of threads to service this completion queue. These must
    // remain running until all the work is done.
    std::vector<std::thread> tasks;
    std::generate_n(std::back_inserter(tasks), 4, [&cq] {
      return std::thread([cq]() mutable { cq.Run(); });
    });

    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic), pubsub::PublisherOptions{},
        pubsub::ConnectionOptions{}.DisableBackgroundThreads(cq)));

    std::vector<future<void>> ids;
    for (char const* data : {"1", "2", "3", "go!"}) {
      ids.push_back(
          publisher.Publish(pubsub::MessageBuilder().SetData(data).Build())
              .then([data](future<StatusOr<std::string>> f) {
                auto s = f.get();
                if (!s) return;
                std::cout << "Sent '" << data << "' (" << *s << ")\n";
              }));
    }
    publisher.Flush();
    // Block until they are actually sent.
    for (auto& id : ids) id.get();

    // Shutdown the completion queue and join the threads
    cq.Shutdown();
    for (auto& t : tasks) t.join();
  }
  //! [custom-thread-pool-publisher]
  (argv.at(0), argv.at(1));
}

void PublisherConcurrencyControl(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "publisher-concurrency-control <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_concurrency_control]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // Override the default number of background (I/O) threads. By default the
    // library uses `std::thread::hardware_concurrency()` threads.
    auto options =
        pubsub::ConnectionOptions{}.set_background_thread_pool_size(8);
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic), pubsub::PublisherOptions{}, std::move(options)));

    std::vector<future<void>> ids;
    for (char const* data : {"1", "2", "3", "go!"}) {
      ids.push_back(
          publisher.Publish(pubsub::MessageBuilder().SetData(data).Build())
              .then([data](future<StatusOr<std::string>> f) {
                auto s = f.get();
                if (!s) return;
                std::cout << "Sent '" << data << "' (" << *s << ")\n";
              }));
    }
    publisher.Flush();
    // Block until they are actually sent.
    for (auto& id : ids) id.get();
  }
  //! [END pubsub_publisher_concurrency_control]
  (argv.at(0), argv.at(1));
}

void PublisherRetrySettings(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{"publisher-retry-settings <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_retry_settings] [publisher-retry-settings]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // By default a publisher will retry for 60 seconds, with an initial backoff
    // of 100ms, a maximum backoff of 60 seconds, and the backoff will grow by
    // 30% after each attempt. This changes those defaults.
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic), pubsub::PublisherOptions{}, {},
        pubsub::LimitedTimeRetryPolicy(
            /*maximum_duration=*/std::chrono::minutes(10))
            .clone(),
        pubsub::ExponentialBackoffPolicy(
            /*initial_delay=*/std::chrono::milliseconds(200),
            /*maximum_delay=*/std::chrono::seconds(45),
            /*scaling=*/2.0)
            .clone()));

    std::vector<future<bool>> done;
    for (char const* data : {"1", "2", "3", "go!"}) {
      done.push_back(
          publisher.Publish(pubsub::MessageBuilder().SetData(data).Build())
              .then([](future<StatusOr<std::string>> f) {
                return f.get().ok();
              }));
    }
    publisher.Flush();
    int count = 0;
    for (auto& f : done) {
      if (f.get()) ++count;
    }
    std::cout << count << " messages sent successfully\n";
  }
  //! [END pubsub_publisher_retry_settings] [publisher-retry-settings]
  (argv.at(0), argv.at(1));
}

void PublisherDisableRetries(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{"publisher-disable-retries <project-id> <topic-id>"};
  }
  //! [publisher-disable-retries]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic), pubsub::PublisherOptions{}, {},
        pubsub::LimitedErrorCountRetryPolicy(/*maximum_failures=*/0).clone(),
        pubsub::ExponentialBackoffPolicy(
            /*initial_delay=*/std::chrono::milliseconds(200),
            /*maximum_delay=*/std::chrono::seconds(45),
            /*scaling=*/2.0)
            .clone()));

    std::vector<future<bool>> done;
    for (char const* data : {"1", "2", "3", "go!"}) {
      done.push_back(
          publisher.Publish(pubsub::MessageBuilder().SetData(data).Build())
              .then([](future<StatusOr<std::string>> f) {
                return f.get().ok();
              }));
    }
    publisher.Flush();
    int count = 0;
    for (auto& f : done) {
      if (f.get()) ++count;
    }
    std::cout << count << " messages sent successfully\n";
  }
  //! [publisher-disable-retries]
  (argv.at(0), argv.at(1));
}

void CustomBatchPublisher(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "custom-thread-pool-publisher <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_batch_settings] [publisher-options]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // By default the publisher will flush a batch after 10ms, after it contains
    // more than 100 message, or after it contains more than 1MiB of data,
    // whichever comes first. This changes those defaults.
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        pubsub::PublisherOptions{}
            .set_maximum_hold_time(std::chrono::milliseconds(20))
            .set_maximum_batch_bytes(4 * 1024 * 1024L)
            .set_maximum_batch_message_count(200),
        pubsub::ConnectionOptions{}));

    std::vector<future<void>> ids;
    for (char const* data : {"1", "2", "3", "go!"}) {
      ids.push_back(
          publisher.Publish(pubsub::MessageBuilder().SetData(data).Build())
              .then([data](future<StatusOr<std::string>> f) {
                auto s = f.get();
                if (!s) return;
                std::cout << "Sent '" << data << "' (" << *s << ")\n";
              }));
    }
    publisher.Flush();
    // Block until they are actually sent.
    for (auto& id : ids) id.get();
  }
  //! [END pubsub_publisher_batch_settings] [publisher-options]
  (argv.at(0), argv.at(1));
}

void CustomThreadPoolSubscriber(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "custom-thread-pool-subscriber <project-id> <topic-id>"};
  }
  //! [custom-thread-pool-subscriber]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string subscription_id) {
    // Create our own completion queue to run the background activity.
    google::cloud::CompletionQueue cq;
    // Setup one or more of threads to service this completion queue. These must
    // remain running until all the work is done.
    std::vector<std::thread> tasks;
    std::generate_n(std::back_inserter(tasks), 4, [&cq] {
      return std::thread([cq]() mutable { cq.Run(); });
    });

    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        pubsub::SubscriberOptions{},
        pubsub::ConnectionOptions{}.DisableBackgroundThreads(cq)));

    // Because this is an example we want to exit eventually, use a mutex and
    // condition variable to notify the current thread and stop the example.
    std::mutex mu;
    std::condition_variable cv;
    int count = 0;
    auto await_count = [&] {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [&] { return count >= 4; });
    };
    auto increase_count = [&] {
      std::unique_lock<std::mutex> lk(mu);
      if (++count >= 4) cv.notify_one();
    };

    // Receive messages in the previously allocated thread pool.
    auto session = subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          increase_count();
          std::move(h).ack();
        });
    await_count();
    session.cancel();
    // Report any final status, blocking until the subscription loop completes,
    // either with a failure or because it was canceled.
    auto status = session.get();
    std::cout << "Message count=" << count << ", status=" << status << "\n";

    // Shutdown the completion queue and join the threads
    cq.Shutdown();
    for (auto& t : tasks) t.join();
  }
  //! [custom-thread-pool-subscriber]
  (argv.at(0), argv.at(1));
}

void SubscriberConcurrencyControl(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "subscriber-concurrency-control <project-id> <subscription-id>"};
  }

  auto const initial = EventCounter::Instance().Current();
  //! [START pubsub_subscriber_concurrency_control] [subscriber-concurrency]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  auto sample = [](std::string project_id, std::string subscription_id) {
    // Create a subscriber with 16 threads handling I/O work, by default the
    // library creates `std::thread::hardware_concurrency()` threads.
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        pubsub::SubscriberOptions{}.set_max_concurrency(8),
        pubsub::ConnectionOptions{}.set_background_thread_pool_size(16)));

    // Create a subscription where up to 8 messages are handled concurrently. By
    // default the library uses `std::thread::hardware_concurrency()` as the
    // maximum number of concurrent callbacks.
    auto session = subscriber.Subscribe(
        [](pubsub::Message const& m, pubsub::AckHandler h) {
          // This handler executes in the I/O threads, applications could use,
          // std::async(), a thread-pool, or any other mechanism to transfer the
          // execution to other threads.
          std::cout << "Received message " << m << "\n";
          std::move(h).ack();
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
    return std::make_pair(subscriber, std::move(session));
  };
  //! [END pubsub_subscriber_concurrency_control] [subscriber-concurrency]
  auto p = sample(argv.at(0), argv.at(1));
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) {
        auto constexpr kExpectedMessageCount = 4;
        return count >= initial + kExpectedMessageCount;
      },
      std::move(p.second), __func__);
}

void SubscriberFlowControlSettings(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "subscriber-retry-settings <project-id> <subscription-id>"};
  }

  auto const initial = EventCounter::Instance().Current();
  //! [START pubsub_subscriber_flow_settings] [subscriber-flow-control]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  auto sample = [](std::string project_id, std::string subscription_id) {
    // Change the flow control watermarks, by default the client library uses
    // 0 and 1,000 for the message count watermarks, and 0 and 10MiB for the
    // size watermarks. Recall that the library stops requesting messages if
    // any of the high watermarks are reached, and the library resumes
    // requesting messages when *both* low watermarks are reached.
    auto constexpr kMiB = 1024 * 1024L;
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        pubsub::SubscriberOptions{}
            .set_max_outstanding_messages(1000)
            .set_max_outstanding_bytes(8 * kMiB)));

    auto session = subscriber.Subscribe(
        [](pubsub::Message const& m, pubsub::AckHandler h) {
          std::move(h).ack();
          std::cout << "Received message " << m << "\n";
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
    return std::make_pair(subscriber, std::move(session));
  };
  //! [END pubsub_subscriber_flow_settings] [subscriber-flow-control]
  auto p = sample(argv.at(0), argv.at(1));
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) {
        auto constexpr kExpectedMessageCount = 4;
        return count >= initial + kExpectedMessageCount;
      },
      std::move(p.second), __func__);
}

void SubscriberRetrySettings(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "subscriber-retry-settings <project-id> <subscription-id>"};
  }

  auto const initial = EventCounter::Instance().Current();
  //! [subscriber-retry-settings]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  auto sample = [](std::string project_id, std::string subscription_id) {
    // By default a subscriber will retry for 60 seconds, with an initial
    // backoff of 100ms, a maximum backoff of 60 seconds, and the backoff will
    // grow by 30% after each attempt. This changes those defaults.
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        pubsub::SubscriberOptions{}, pubsub::ConnectionOptions{},
        pubsub::LimitedTimeRetryPolicy(
            /*maximum_duration=*/std::chrono::minutes(1))
            .clone(),
        pubsub::ExponentialBackoffPolicy(
            /*initial_delay=*/std::chrono::milliseconds(200),
            /*maximum_delay=*/std::chrono::seconds(10),
            /*scaling=*/2.0)
            .clone()));

    auto session = subscriber.Subscribe(
        [](pubsub::Message const& m, pubsub::AckHandler h) {
          std::move(h).ack();
          std::cout << "Received message " << m << "\n";
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
    return std::make_pair(subscriber, std::move(session));
  };
  //! [subscriber-retry-settings]
  auto p = sample(argv.at(0), argv.at(1));
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) {
        auto constexpr kExpectedMessageCount = 1;
        return count >= initial + kExpectedMessageCount;
      },
      std::move(p.second), __func__);
}

void AutoRunAvro(
    std::string const& project_id,
    google::cloud::internal::DefaultPRNG& generator,
    google::cloud::pubsub::TopicAdminClient& topic_admin_client,
    google::cloud::pubsub::SubscriptionAdminClient& subscription_admin_client) {
  auto schema_admin = google::cloud::pubsub::SchemaAdminClient(
      google::cloud::pubsub::MakeSchemaAdminConnection());
  auto avro_schema_id = RandomSchemaId(generator);
  std::cout << "\nRunning CreateAvroSchema() sample" << std::endl;
  CreateAvroSchema(schema_admin, {project_id, avro_schema_id});

  std::cout << "\nRunning GetSchema sample" << std::endl;
  GetSchema(schema_admin, {project_id, avro_schema_id});
  std::cout << "\nRunning ListSchemas() sample" << std::endl;
  ListSchemas(schema_admin, {project_id});

  std::cout << "\nRunning ValidateAvroMessage() sample" << std::endl;
  ValidateMessageAvro(schema_admin, {project_id});

  std::cout << "\nRunning CreateTopicWithSchema() sample [avro]" << std::endl;
  auto const avro_topic_id = RandomTopicId(generator);
  CreateTopicWithSchema(topic_admin_client,
                        {project_id, avro_topic_id, avro_schema_id, "JSON"});

  auto topic = google::cloud::pubsub::Topic(project_id, avro_topic_id);
  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::pubsub::PublisherOptions{}
                     .set_maximum_batch_message_count(1)));

  auto const subscription_id = RandomSubscriptionId(generator);
  auto subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(subscription));

  std::cout << "\nRunning CreateSubscription() sample [avro]" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, avro_topic_id, subscription_id});

  std::cout << "\nRunning SubscribeAvroRecords() sample" << std::endl;
  auto session = SubscribeAvroRecords(subscriber, {});

  std::cout << "\nRunning PublishAvroRecords() sample" << std::endl;
  PublishAvroRecords(publisher, {});

  session.cancel();
  WaitForSession(std::move(session), "SubscribeAvroRecords");

  std::cout << "\nRunning DeleteSubscription() sample [avro]" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteTopic() sample [avro]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, avro_topic_id});

  std::cout << "\nRunning DeleteSchema() sample [avro]" << std::endl;
  DeleteSchema(schema_admin, {project_id, avro_schema_id});
}

void AutoRunProtobuf(
    std::string const& project_id,
    google::cloud::internal::DefaultPRNG& generator,
    google::cloud::pubsub::TopicAdminClient& topic_admin_client,
    google::cloud::pubsub::SubscriptionAdminClient& subscription_admin_client) {
  auto schema_admin = google::cloud::pubsub::SchemaAdminClient(
      google::cloud::pubsub::MakeSchemaAdminConnection());

  std::cout << "\nRunning ValidateProtobufSchema() sample" << std::endl;
  ValidateProtobufSchema(schema_admin, {project_id});

  std::cout << "\nRunning ValidateProtobufMessage() sample" << std::endl;
  ValidateMessageProtobuf(schema_admin, {project_id});

  auto proto_schema_id = RandomSchemaId(generator);
  std::cout << "\nRunning CreateProtobufSchema() sample" << std::endl;
  CreateProtobufSchema(schema_admin, {project_id, proto_schema_id});

  std::cout << "\nRunning ValidateMessageNamed() sample" << std::endl;
  ValidateMessageNamedSchema(schema_admin, {project_id, proto_schema_id});

  // TODO(#4792) - the CreateSchema() operation would have failed, what follows
  //    would fail and stop all the other examples.
  if (google::cloud::pubsub::examples::UsingEmulator()) return;

  std::cout << "\nRunning CreateTopicWithSchema() sample [proto]" << std::endl;
  auto const proto_topic_id = RandomTopicId(generator);
  CreateTopicWithSchema(topic_admin_client, {project_id, proto_topic_id,
                                             proto_schema_id, "BINARY"});

  auto topic = google::cloud::pubsub::Topic(project_id, proto_topic_id);
  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::pubsub::PublisherOptions{}
                     .set_maximum_batch_message_count(1)));

  auto const subscription_id = RandomSubscriptionId(generator);
  auto subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(subscription));

  std::cout << "\nRunning CreateSubscription sample [proto]" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, proto_topic_id, subscription_id});

  std::cout << "\nRunning CreateSubscription() sample [avro]" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, proto_topic_id, subscription_id});

  std::cout << "\nRunning SubscribeProtobufRecords() sample" << std::endl;
  auto session = SubscribeProtobufRecords(subscriber, {});

  std::cout << "\nRunning PublishProtobufRecords() sample" << std::endl;
  PublishProtobufRecords(publisher, {});

  session.cancel();
  WaitForSession(std::move(session), "SubscribeProtobufRecords");

  std::cout << "\nRunning DeleteSubscription sample [proto]" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteTopic() sample [proto]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, proto_topic_id});

  std::cout << "\nRunning DeleteSchema() sample [proto]" << std::endl;
  DeleteSchema(schema_admin, {project_id, proto_schema_id});
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  using ::google::cloud::pubsub::examples::UsingEmulator;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const subscription_id = RandomSubscriptionId(generator);
  auto const filtered_subscription_id = RandomSubscriptionId(generator);
  auto const push_subscription_id = RandomSubscriptionId(generator);
  auto const ordering_subscription_id = RandomSubscriptionId(generator);
  auto const ordering_topic_id = "ordering-" + RandomTopicId(generator);
  auto const dead_letter_subscription_id = RandomSubscriptionId(generator);
  auto const dead_letter_topic_id = "dead-letter-" + RandomTopicId(generator);

  auto const snapshot_id = RandomSnapshotId(generator);

  google::cloud::pubsub::TopicAdminClient topic_admin_client(
      google::cloud::pubsub::MakeTopicAdminConnection());
  google::cloud::pubsub::SubscriptionAdminClient subscription_admin_client(
      google::cloud::pubsub::MakeSubscriptionAdminConnection());

  std::cout << "\nRunning CreateTopic() sample [1]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning CreateTopic() sample [2]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning CreateTopic() sample [3]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, ordering_topic_id});

  std::cout << "\nRunning GetTopic() sample" << std::endl;
  GetTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning UpdateTopic() sample" << std::endl;
  UpdateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning the StatusOr example" << std::endl;
  ExampleStatusOr(topic_admin_client, {project_id});

  std::cout << "\nRunning ListTopics() sample" << std::endl;
  ListTopics(topic_admin_client, {project_id});

  std::cout << "\nRunning CreateSubscription() sample [1]" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, topic_id, subscription_id});

  std::cout << "\nRunning CreateSubscription() sample [2]" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, topic_id, subscription_id});

  std::cout << "\nRunning CreateFilteredSubscription() sample [1]" << std::endl;
  CreateFilteredSubscription(subscription_admin_client,
                             {project_id, topic_id, filtered_subscription_id});

  std::cout << "\nRunning CreateFilteredSubscription() sample [2]" << std::endl;
  CreateFilteredSubscription(subscription_admin_client,
                             {project_id, topic_id, filtered_subscription_id});

  std::cout << "\nRunning ListTopicSubscriptions() sample" << std::endl;
  ListTopicSubscriptions(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning GetSubscription() sample" << std::endl;
  GetSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning UpdateSubscription() sample" << std::endl;
  UpdateSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning ListSubscriptions() sample" << std::endl;
  ListSubscriptions(subscription_admin_client, {project_id});

  auto const endpoint1 = "https://" + project_id + ".appspot.com/push1";
  auto const endpoint2 = "https://" + project_id + ".appspot.com/push2";
  std::cout << "\nRunning CreatePushSubscription() sample [1]" << std::endl;
  CreatePushSubscription(
      subscription_admin_client,
      {project_id, topic_id, push_subscription_id, endpoint1});

  std::cout << "\nRunning CreatePushSubscription() sample [2]" << std::endl;
  CreatePushSubscription(
      subscription_admin_client,
      {project_id, topic_id, push_subscription_id, endpoint1});

  std::cout << "\nRunning CreateOrderingSubscription() sample" << std::endl;
  CreateOrderingSubscription(
      subscription_admin_client,
      {project_id, ordering_topic_id, ordering_subscription_id});

  std::cout << "\nRunning ModifyPushConfig() sample" << std::endl;
  ModifyPushConfig(subscription_admin_client,
                   {project_id, push_subscription_id, endpoint2});

  std::cout << "\nRunning DeleteSubscription() sample [1]" << std::endl;
  // Move push_subscription_id to prevent accidentally using it below.
  DeleteSubscription(subscription_admin_client,
                     {project_id, std::move(push_subscription_id)});

  std::cout << "\nRunning CreateTopic() sample [4]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, dead_letter_topic_id});

  // Hardcode this number as it does not really matter. The other samples pick
  // something between 10 and 15.
  auto constexpr kDeadLetterDeliveryAttempts = 15;

  std::cout << "\nRunning CreateDeadLetterSubscription() sample" << std::endl;
  CreateDeadLetterSubscription(
      subscription_admin_client,
      {project_id, topic_id, dead_letter_subscription_id, dead_letter_topic_id,
       std::to_string(kDeadLetterDeliveryAttempts)});

  auto constexpr kUpdatedDeadLetterDeliveryAttempts = 20;

  std::cout << "\nRunning UpdateDeadLetterSubscription() sample" << std::endl;
  UpdateDeadLetterSubscription(
      subscription_admin_client,
      {project_id, dead_letter_subscription_id, dead_letter_topic_id,
       std::to_string(kUpdatedDeadLetterDeliveryAttempts)});

  std::cout << "\nRunning CreateSnapshot() sample [1]" << std::endl;
  CreateSnapshot(subscription_admin_client,
                 {project_id, subscription_id, snapshot_id});

  std::cout << "\nRunning CreateSnapshot() sample [2]" << std::endl;
  CreateSnapshot(subscription_admin_client,
                 {project_id, subscription_id, snapshot_id});

  std::cout << "\nRunning ListTopicSnapshots() sample" << std::endl;
  ListTopicSnapshots(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning GetSnapshot() sample" << std::endl;
  GetSnapshot(subscription_admin_client, {project_id, snapshot_id});

  std::cout << "\nRunning UpdateSnapshot() sample" << std::endl;
  UpdateSnapshot(subscription_admin_client, {project_id, snapshot_id});

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

  AutoRunAvro(project_id, generator, topic_admin_client,
              subscription_admin_client);
  AutoRunProtobuf(project_id, generator, topic_admin_client,
                  subscription_admin_client);

  auto topic = google::cloud::pubsub::Topic(project_id, topic_id);
  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::pubsub::PublisherOptions{}
                     .set_maximum_batch_message_count(1)));
  auto subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(subscription));

  auto dead_letter_subscription = google::cloud::pubsub::Subscription(
      project_id, dead_letter_subscription_id);
  auto dead_letter_subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(
          dead_letter_subscription));

  auto filtered_subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(
          google::cloud::pubsub::Subscription(project_id,
                                              filtered_subscription_id)));

  std::cout << "\nRunning Publish() sample [1]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning PublishCustomAttributes() sample [1]" << std::endl;
  PublishCustomAttributes(publisher, {});

  std::cout << "\nRunning Subscribe() sample" << std::endl;
  Subscribe(subscriber, {});

  std::cout << "\nRunning Subscribe(filtered) sample" << std::endl;
  PublishHelper(publisher, "Subscribe(filtered)", 8);
  Subscribe(filtered_subscriber, {});

  std::cout << "\nRunning Publish() sample [2]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning SubscribeErrorListener() sample" << std::endl;
  SubscribeErrorListener(subscriber, {});

  std::cout << "\nRunning Publish() sample [3]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning ReceiveDeadLetterDeliveryAttempt() sample"
            << std::endl;
  ReceiveDeadLetterDeliveryAttempt(dead_letter_subscriber, {});

  std::cout << "\nRunning RemoveDeadLetterPolicy() sample" << std::endl;
  RemoveDeadLetterPolicy(subscription_admin_client,
                         {project_id, dead_letter_subscription_id});

  std::cout << "\nRunning the CustomThreadPoolPublisher() sample" << std::endl;
  CustomThreadPoolPublisher({project_id, topic_id});

  std::cout << "\nRunning the PublisherConcurrencyControl() sample"
            << std::endl;
  PublisherConcurrencyControl({project_id, topic_id});

  std::cout << "\nRunning the PublisherRetrySettings() sample" << std::endl;
  PublisherRetrySettings({project_id, topic_id});

  std::cout << "\nRunning the PublisherDisableRetries() sample" << std::endl;
  PublisherDisableRetries({project_id, topic_id});

  std::cout << "\nRunning the CustomBatchPublisher() sample" << std::endl;
  CustomBatchPublisher({project_id, topic_id});

  std::cout << "\nRunning the CustomThreadPoolSubscriber() sample" << std::endl;
  CustomThreadPoolSubscriber({project_id, subscription_id});

  std::cout << "\nRunning PublishCustomAttributes() sample [2]" << std::endl;
  PublishCustomAttributes(publisher, {});

  std::cout << "\nRunning SubscribeCustomAttributes() sample" << std::endl;
  SubscribeCustomAttributes(subscriber, {});

  auto publisher_with_ordering_key = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::pubsub::PublisherOptions{}
                     .set_maximum_batch_message_count(1)
                     .enable_message_ordering()));
  std::cout << "\nRunning PublishOrderingKey() sample" << std::endl;
  PublishOrderingKey(publisher_with_ordering_key, {});

  std::cout << "\nRunning ResumeOrderingKey() sample" << std::endl;
  ResumeOrderingKey(publisher_with_ordering_key, {});

  std::cout << "\nRunning Publish() sample [4]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning SubscriberConcurrencyControl() sample" << std::endl;
  PublishHelper(publisher, "SubscriberConcurrentControl", 4);
  SubscriberConcurrencyControl({project_id, subscription_id});

  std::cout << "\nRunning SubscriberFlowControlSettings() sample" << std::endl;
  PublishHelper(publisher, "SubscriberFlowControlSettings", 4);
  SubscriberFlowControlSettings({project_id, subscription_id});

  std::cout << "\nRunning SubscriberRetrySettings() sample" << std::endl;
  PublishHelper(publisher, "SubscriberRetrySettings", 1);
  SubscriberRetrySettings({project_id, subscription_id});

  std::cout << "\nRunning DetachSubscription() sample" << std::endl;
  DetachSubscription(topic_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteSubscription() sample [2]" << std::endl;
  DeleteSubscription(subscription_admin_client,
                     {project_id, std::move(dead_letter_subscription_id)});

  std::cout << "\nRunning DeleteSubscription() sample [3] " << std::endl;
  DeleteSubscription(subscription_admin_client,
                     {project_id, ordering_subscription_id});

  std::cout << "\nRunning DeleteSubscription() sample [4]" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteSubscription() sample [5]" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteTopic() sample [1]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, dead_letter_topic_id});

  std::cout << "\nRunning DeleteTopic() sample [2]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, ordering_topic_id});

  std::cout << "\nRunning DeleteTopic() sample [3]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning DeleteTopic() sample [4]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::pubsub::examples::CreatePublisherCommand;
  using ::google::cloud::pubsub::examples::CreateSchemaAdminCommand;
  using ::google::cloud::pubsub::examples::CreateSubscriberCommand;
  using ::google::cloud::pubsub::examples::CreateSubscriptionAdminCommand;
  using ::google::cloud::pubsub::examples::CreateTopicAdminCommand;
  using ::google::cloud::testing_util::Example;

  Example example({
      CreateTopicAdminCommand("create-topic", {"project-id", "topic-id"},
                              CreateTopic),
      CreateTopicAdminCommand("get-topic", {"project-id", "topic-id"},
                              GetTopic),
      CreateTopicAdminCommand("update-topic", {"project-id", "topic-id"},
                              UpdateTopic),
      CreateTopicAdminCommand("list-topics", {"project-id"}, ListTopics),
      CreateTopicAdminCommand("delete-topic", {"project-id", "topic-id"},
                              DeleteTopic),
      CreateTopicAdminCommand("detach-subscription",
                              {"project-id", "subscription-id"},
                              DetachSubscription),
      CreateTopicAdminCommand("list-topic-subscriptions",
                              {"project-id", "topic-id"},
                              ListTopicSubscriptions),
      CreateTopicAdminCommand("list-topic-snapshots",
                              {"project-id", "topic-id"}, ListTopicSnapshots),
      CreateSubscriptionAdminCommand(
          "create-subscription", {"project-id", "topic-id", "subscription-id"},
          CreateSubscription),
      CreateSubscriptionAdminCommand(
          "create-filtered-subscription",
          {"project-id", "topic-id", "subscription-id"},
          CreateFilteredSubscription),
      CreateSubscriptionAdminCommand(
          "create-push-subscription",
          {"project-id", "topic-id", "subscription-id", "endpoint"},
          CreatePushSubscription),
      CreateSubscriptionAdminCommand(
          "create-ordering-subscription",
          {"project-id", "topic-id", "subscription-id"},
          CreateOrderingSubscription),
      CreateSubscriptionAdminCommand(
          "create-dead-letter-subscription",
          {"project-id", "topic-id", "subscription-id", "dead-letter-topic-id",
           "dead-letter-delivery-attempts"},
          CreateDeadLetterSubscription),
      CreateSubscriptionAdminCommand(
          "update-dead-letter-subscription",
          {"project-id", "subscription-id", "dead-letter-topic-id",
           "dead-letter-delivery-attempts"},
          UpdateDeadLetterSubscription),
      CreateSubscriptionAdminCommand("remove-dead-letter-policy",
                                     {"project-id", "subscription-id"},
                                     RemoveDeadLetterPolicy),
      CreateSubscriptionAdminCommand("get-subscription",
                                     {"project-id", "subscription-id"},
                                     GetSubscription),
      CreateSubscriptionAdminCommand("update-subscription",
                                     {"project-id", "subscription-id"},
                                     UpdateSubscription),
      CreateSubscriptionAdminCommand("list-subscriptions", {"project-id"},
                                     ListSubscriptions),
      CreateSubscriptionAdminCommand("delete-subscription",
                                     {"project-id", "subscription-id"},
                                     DeleteSubscription),
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

      CreateSchemaAdminCommand("create-avro-schema",
                               {"project-id", "schema-id"}, CreateAvroSchema),
      CreateSchemaAdminCommand("create-protobuf-schema",
                               {"project-id", "schema-id"},
                               CreateProtobufSchema),
      CreateSchemaAdminCommand("get-schema", {"project-id", "schema-id"},
                               GetSchema),
      CreateSchemaAdminCommand("list-schemas", {"project-id"}, ListSchemas),
      CreateSchemaAdminCommand("delete-schema", {"project-id", "schema-id"},
                               DeleteSchema),
      CreateSchemaAdminCommand("validate-avro-schema", {"project-id"},
                               ValidateAvroSchema),
      CreateSchemaAdminCommand("validate-protobuf-schema", {"project-id"},
                               ValidateProtobufSchema),
      CreateSchemaAdminCommand("validate-message-avro", {"project-id"},
                               ValidateMessageAvro),
      CreateSchemaAdminCommand("validate-message-protobuf", {"project-id"},
                               ValidateMessageProtobuf),
      CreateSchemaAdminCommand("validate-message-named-schema",
                               {"project-id", "schema-id"},
                               ValidateMessageNamedSchema),
      CreateTopicAdminCommand(
          "create-topic-with-schema",
          {"project-id", "topic-id", "schema-id", "encoding"},
          CreateTopicWithSchema),
      CreatePublisherCommand("publish-avro-records", {}, PublishAvroRecords),
      CreateSubscriberCommand("subscribe-avro-records", {},
                              SubscribeAvroRecords),
      CreatePublisherCommand("publish-protobuf-records", {},
                             PublishProtobufRecords),
      CreateSubscriberCommand("subscribe-protobuf-records", {},
                              SubscribeProtobufRecords),

      CreatePublisherCommand("publish", {}, Publish),
      CreatePublisherCommand("publish-custom-attributes", {},
                             PublishCustomAttributes),
      CreatePublisherCommand("publish-ordering-key", {}, PublishOrderingKey),
      CreatePublisherCommand("resume-ordering-key", {}, ResumeOrderingKey),
      CreateSubscriberCommand("subscribe", {}, Subscribe),
      CreateSubscriberCommand("subscribe-error-listener", {},
                              SubscribeErrorListener),
      CreateSubscriberCommand("subscribe-custom-attributes", {},
                              SubscribeCustomAttributes),
      CreateSubscriberCommand("receive-dead-letter-delivery-attempt", {},
                              ReceiveDeadLetterDeliveryAttempt),
      {"custom-thread-pool-publisher", CustomThreadPoolPublisher},
      {"publisher-concurrency-control", PublisherConcurrencyControl},
      {"publisher-retry-settings", PublisherRetrySettings},
      {"publisher-disable-retry", PublisherDisableRetries},
      {"custom-batch-publisher", CustomBatchPublisher},
      {"custom-thread-pool-subscriber", CustomThreadPoolSubscriber},
      {"subscriber-concurrency-control", SubscriberConcurrencyControl},
      {"subscriber-flow-control-settings", SubscriberFlowControlSettings},
      {"subscriber-retry-settings", SubscriberRetrySettings},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
