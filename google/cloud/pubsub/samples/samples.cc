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
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/example_driver.h"
#include <atomic>
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

void CreateTopic(google::cloud::pubsub::TopicAdminClient client,
                 std::vector<std::string> const& argv) {
  //! [START pubsub_create_topic] [create-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.CreateTopic(pubsub::TopicMutationBuilder(
        pubsub::Topic(std::move(project_id), std::move(topic_id))));
    if (!topic) throw std::runtime_error(topic.status().message());

    std::cout << "The topic was successfully created: " << topic->DebugString()
              << "\n";
  }
  //! [END pubsub_create_topic] [create-topic]
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
        pubsub::TopicMutationBuilder(
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
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The topic was successfully deleted\n";
  }
  //! [END pubsub_delete_topic] [delete-topic]
  (std::move(client), argv.at(0), argv.at(1));
}

void DetachSubscription(google::cloud::pubsub::TopicAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [START pubsub_subscription_detachment] [detach-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string subscription_id) {
    auto response = client.DetachSubscription(pubsub::Subscription(
        std::move(project_id), std::move(subscription_id)));
    if (!response.ok()) return;  // TODO(#4792) - not implemented in emulator

    std::cout << "The subscription was successfully detached: "
              << response->DebugString() << "\n";
  }
  //! [END pubsub_subscription_detachment] [detach-subscription]
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
  //! [START pubsub_list_topic_snapshots] [list-topic-snapshots]
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
  //! [END pubsub_list_topic_snapshots] [list-topic-snapshots]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateSubscription(google::cloud::pubsub::SubscriptionAdminClient client,
                        std::vector<std::string> const& argv) {
  //! [START pubsub_create_pull_subscription] [create-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& topic_id, std::string const& subscription_id) {
    auto subscription = client.CreateSubscription(
        pubsub::Topic(project_id, std::move(topic_id)),
        pubsub::Subscription(project_id, std::move(subscription_id)));
    if (!subscription)
      throw std::runtime_error(subscription.status().message());

    std::cout << "The subscription was successfully created: "
              << subscription->DebugString() << "\n";
  }
  //! [END pubsub_create_pull_subscription] [create-subscription]
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
        pubsub::SubscriptionMutationBuilder{}.set_push_config(
            pubsub::PushConfigBuilder{}.set_push_endpoint(endpoint)));
    if (!sub) throw std::runtime_error(sub.status().message());

    std::cout << "The subscription was successfully created: "
              << sub->DebugString() << "\n";
  }
  //! [END pubsub_create_push_subscription] [create-push-subscription]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetSubscription(google::cloud::pubsub::SubscriptionAdminClient client,
                     std::vector<std::string> const& argv) {
  //! [get-subscription]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::SubscriptionAdminClient client, std::string const& project_id,
     std::string const& subscription_id) {
    auto s = client.GetSubscription(
        pubsub::Subscription(project_id, std::move(subscription_id)));
    if (!s) throw std::runtime_error(s.status().message());

    std::cout << "The subscription exists and its metadata is: "
              << s->DebugString() << "\n";
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
        pubsub::SubscriptionMutationBuilder{}.set_ack_deadline(
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
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The subscription was successfully deleted\n";
  }
  //! [END pubsub_delete_subscription] [delete-subscription]
  (std::move(client), argv.at(0), argv.at(1));
}

void ModifyPushConfig(google::cloud::pubsub::SubscriptionAdminClient client,
                      std::vector<std::string> const& argv) {
  //! [START pubsub_update_push_subscription] [modify-push-config]
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
  //! [END pubsub_update_push_subscription] [modify-push-config]
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
        pubsub::SnapshotMutationBuilder{}.add_label("samples-cpp", "gcp"));
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
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The snapshot was successfully deleted\n";
  }
  //! [delete-snapshot]
  (std::move(client), argv.at(0), argv.at(1));
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
      // Use `topic` like a smart pointer; check it before dereferencing
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

void Publish(google::cloud::pubsub::Publisher publisher,
             std::vector<std::string> const&) {
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
              .SetAttributes({{"origin", "cpp-sample"}, {"username", "gcp"}})
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

void Subscribe(google::cloud::pubsub::Subscriber subscriber,
               google::cloud::pubsub::Subscription const& subscription,
               std::vector<std::string> const&) {
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  //! [START pubsub_subscriber_async_pull] [subscribe]
  [](pubsub::Subscriber subscriber, pubsub::Subscription const& subscription) {
    std::atomic<int> count{0};
    google::cloud::promise<void> received_message;
    auto result = subscriber.Subscribe(
        subscription, [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          // Signal the waiting thread to detach the subscription after the
          // first message.
          if (count.load() == 0) received_message.set_value();
          ++count;
          // Only then ack the message, preventing any new requests from being
          // issued.
          std::move(h).ack();
        });
    // Cancel the subscription as soon as the first message is received, and
    // block until that happens.
    received_message.get_future()
        .then([&result](google::cloud::future<void>) { result.cancel(); })
        .get();
    // Report any final status, blocking.
    std::cout << "Message count = " << count.load()
              << ", status = " << result.get() << "\n";
  }
  //! [END pubsub_subscriber_async_pull] [subscribe]
  (std::move(subscriber), std::move(subscription));
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
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        pubsub::PublisherOptions{}
            .set_maximum_hold_time(std::chrono::milliseconds(10))
            .set_maximum_batch_bytes(10 * 1024 * 1024L)
            .set_maximum_message_count(100),
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
    // Create our own completion queue to run the background activity, such as
    // flushing the publisher.
    google::cloud::CompletionQueue cq;
    // Setup one or more of threads to service this completion queue. These must
    // remain running until all the work is done.
    std::vector<std::thread> tasks;
    std::generate_n(std::back_inserter(tasks), 4, [&cq] {
      return std::thread([cq]() mutable { cq.Run(); });
    });

    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::ConnectionOptions{}.DisableBackgroundThreads(cq)));
    auto subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));

    std::mutex mu;
    int count = 0;
    google::cloud::promise<void> received_message;
    auto result = subscriber.Subscribe(
        subscription, [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << std::endl;
          {
            std::lock_guard<std::mutex> lk(mu);
            if (++count == 4) received_message.set_value();
          }
          std::move(h).ack();
        });
    // Cancel the subscription as soon as the first message is received.
    received_message.get_future()
        .then([&result](google::cloud::future<void>) { result.cancel(); })
        .get();
    // Report any final status, blocking until the subscription loop completes,
    // either with a failure or because it was canceled.
    std::cout << "Message count=" << count << ", status=" << result.get()
              << "\n";

    // Shutdown the completion queue and join the threads
    cq.Shutdown();
    for (auto& t : tasks) t.join();
  }
  //! [custom-thread-pool-subscriber]
  (argv.at(0), argv.at(1));
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
  auto push_subscription_id = RandomSubscriptionId(generator);

  auto snapshot_id = RandomSnapshotId(generator);

  google::cloud::pubsub::TopicAdminClient topic_admin_client(
      google::cloud::pubsub::MakeTopicAdminConnection());
  google::cloud::pubsub::SubscriptionAdminClient subscription_admin_client(
      google::cloud::pubsub::MakeSubscriptionAdminConnection());

  std::cout << "\nRunning CreateTopic() sample" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning GetTopic() sample" << std::endl;
  GetTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning UpdateTopic() sample" << std::endl;
  UpdateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning the StatusOr example" << std::endl;
  ExampleStatusOr(topic_admin_client, {project_id});

  std::cout << "\nRunning ListTopics() sample" << std::endl;
  ListTopics(topic_admin_client, {project_id});

  std::cout << "\nRunning CreateSubscription() sample" << std::endl;
  CreateSubscription(subscription_admin_client,
                     {project_id, topic_id, subscription_id});

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
  std::cout << "\nRunning CreatePushSubscription() sample" << std::endl;
  CreatePushSubscription(
      subscription_admin_client,
      {project_id, topic_id, push_subscription_id, endpoint1});

  std::cout << "\nRunning ModifyPushConfig() sample" << std::endl;
  ModifyPushConfig(subscription_admin_client,
                   {project_id, push_subscription_id, endpoint2});

  std::cout << "\nRunning DeleteSubscription() sample [1]" << std::endl;
  // Move push_subscription_id to prevent accidentally using it below.
  DeleteSubscription(subscription_admin_client,
                     {project_id, std::move(push_subscription_id)});

  std::cout << "\nRunning CreateSnapshot() sample" << std::endl;
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

  std::cout << "\nRunning DeleteSnapshot() sample" << std::endl;
  DeleteSnapshot(subscription_admin_client, {project_id, snapshot_id});

  auto topic = google::cloud::pubsub::Topic(project_id, topic_id);
  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic,
          google::cloud::pubsub::PublisherOptions{}.set_maximum_message_count(
              1)));
  auto subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection());

  std::cout << "\nRunning Publish() sample" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning PublishCustomAttributes() sample" << std::endl;
  PublishCustomAttributes(publisher, {});

  std::cout << "\nRunning Subscribe() sample" << std::endl;
  Subscribe(subscriber, subscription, {});

  std::cout << "\nRunning the CustomThreadPoolPublisher() sample" << std::endl;
  CustomThreadPoolPublisher({project_id, topic_id});

  std::cout << "\nRunning the CustomBatchPublisher() sample" << std::endl;
  CustomBatchPublisher({project_id, topic_id});

  std::cout << "\nRunning the CustomThreadPoolSubscriber() sample" << std::endl;
  CustomThreadPoolSubscriber({project_id, subscription_id});

  std::cout << "\nRunning DetachSubscription() sample" << std::endl;
  DetachSubscription(topic_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteSubscription() sample" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning delete-topic sample" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, topic_id});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::pubsub::examples::CreatePublisherCommand;
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
          "create-push-subscription",
          {"project-id", "topic-id", "subscription-id", "endpoint"},
          CreatePushSubscription),
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
      CreatePublisherCommand("publish", {}, Publish),
      CreatePublisherCommand("publish-custom-attributes", {},
                             PublishCustomAttributes),
      CreateSubscriberCommand("subscribe", {}, Subscribe),
      {"custom-thread-pool-publisher", CustomThreadPoolPublisher},
      {"custom-batch-publisher", CustomBatchPublisher},
      {"custom-thread-pool-subscriber", CustomThreadPoolSubscriber},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
