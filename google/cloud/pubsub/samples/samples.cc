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
  //! [START pubsub_quickstart_create_topic]
  //! [START pubsub_create_topic] [create-topic]
  namespace pubsub = google::cloud::pubsub;
  [](pubsub::TopicAdminClient client, std::string project_id,
     std::string topic_id) {
    auto topic = client.CreateTopic(pubsub::TopicMutationBuilder(
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
  //! [START pubsub_quickstart_subscriber]
  //! [START pubsub_subscriber_async_pull] [subscribe]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Subscriber subscriber, pubsub::Subscription const& subscription) {
    std::mutex mu;
    std::condition_variable cv;
    int message_count = 0;
    auto session = subscriber.Subscribe(
        subscription, [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          std::unique_lock<std::mutex> lk(mu);
          ++message_count;
          lk.unlock();
          cv.notify_one();
          std::move(h).ack();
        });
    // Wait until at least one message has been received.
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&message_count] { return message_count > 0; });
    lk.unlock();
    // Cancel the subscription session.
    session.cancel();
    // Wait for the session to complete, no more callbacks can happen after this
    // point.
    auto status = session.get();
    // Report any final status, blocking.
    std::cout << "Message count: " << message_count << ", status: " << status
              << "\n";
  }
  //! [END pubsub_subscriber_async_pull] [subscribe]
  //! [END pubsub_quickstart_subscriber]
  (std::move(subscriber), std::move(subscription));
}

void SubscribeErrorListener(
    google::cloud::pubsub::Subscriber subscriber,
    google::cloud::pubsub::Subscription const& subscription,
    std::vector<std::string> const&) {
  // [START pubsub_subscriber_error_listener]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Subscriber subscriber, pubsub::Subscription const& subscription) {
    std::mutex mu;
    std::condition_variable cv;
    bool done = false;
    int message_count = 0;
    auto session =
        subscriber
            .Subscribe(subscription,
                       [&](pubsub::Message const& m, pubsub::AckHandler h) {
                         std::cout << "Received message " << m << "\n";
                         std::unique_lock<std::mutex> lk(mu);
                         ++message_count;
                         done = true;
                         lk.unlock();
                         cv.notify_one();
                         std::move(h).ack();
                       })
            // Setup an error handler for the subscription session
            .then([&](future<google::cloud::Status> f) {
              std::cout << "Subscription session result: " << f.get() << "\n";
              std::unique_lock<std::mutex> lk(mu);
              done = true;
              cv.notify_one();
            });
    // Most applications would just release the `session` object at this point,
    // but we want to gracefully close down this example.
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&done] { return done; });
    lk.unlock();
    session.cancel();
    session.get();
    std::cout << "Message count:" << message_count << "\n";
  }
  // [END pubsub_subscriber_error_listener]
  (std::move(subscriber), std::move(subscription));
}

void SubscribeCustomAttributes(
    google::cloud::pubsub::Subscriber subscriber,
    google::cloud::pubsub::Subscription const& subscription,
    std::vector<std::string> const&) {
  //! [START pubsub_subscriber_async_pull_custom_attributes]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Subscriber subscriber, pubsub::Subscription const& subscription) {
    std::mutex mu;
    std::condition_variable cv;
    int message_count = 0;
    auto session = subscriber.Subscribe(
        subscription, [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message with attributes:\n";
          for (auto const& kv : m.attributes()) {
            std::cout << "  " << kv.first << ": " << kv.second << "\n";
          }
          std::unique_lock<std::mutex> lk(mu);
          ++message_count;
          lk.unlock();
          cv.notify_one();
          std::move(h).ack();
        });
    // Most applications would just release the `session` object at this point,
    // but we want to gracefully close down this example.
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&message_count] { return message_count > 0; });
    lk.unlock();
    session.cancel();
    auto status = session.get();
    std::cout << "Message count: " << message_count << ", status: " << status
              << "\n";
  }
  //! [END pubsub_subscriber_async_pull_custom_attributes]
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
        std::move(topic),
        pubsub::PublisherOptions{}.enable_retry_publish_failures(), {},
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
            .set_maximum_message_count(200),
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
          std::cout << "Received message " << m << "\n";
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
    auto status = result.get();
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

  //! [START pubsub_subscriber_concurrency_control] [subscriber-concurrency]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string subscription_id) {
    // Create a subscriber with 16 threads handling I/O work, by default the
    // library creates `std::thread::hardware_concurrency()` threads.
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::ConnectionOptions{}.set_background_thread_pool_size(16)));
    auto subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));

    std::mutex mu;
    std::condition_variable cv;
    int count = 0;
    auto constexpr kExpectedMessageCount = 4;
    auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
      // This handler executes in the I/O threads, applications could use,
      // std::async(), a thread-pool,
      // google::cloud::CompletionQueue::RunAsync(), or any other mechanism to
      // transfer the execution to other threads.
      std::cout << "Received message " << m << "\n";
      std::move(h).ack();
      {
        std::lock_guard<std::mutex> lk(mu);
        if (++count < kExpectedMessageCount) return;
      }
      cv.notify_one();
    };

    // Create a subscription where up to 8 messages are handled concurrently. By
    // default the library uses `0` and `std::thread::hardwarde_concurrency()`
    // for the concurrency watermarks.
    auto session = subscriber.Subscribe(
        subscription, std::move(handler),
        pubsub::SubscriptionOptions{}.set_concurrency_watermarks(
            /*lwm=*/4, /*hwm=*/8));
    {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [&] { return count >= kExpectedMessageCount; });
    }
    session.cancel();
    auto status = session.get();
    std::cout << "Message count: " << count << ", status: " << status << "\n";
  }
  //! [END pubsub_subscriber_concurrency_control] [subscriber-concurrency]
  (argv.at(0), argv.at(1));
}

void SubscriberFlowControlSettings(
    google::cloud::pubsub::Subscriber subscriber,
    google::cloud::pubsub::Subscription const& subscription,
    std::vector<std::string> const&) {
  //! [START pubsub_subscriber_flow_settings] [subscriber-flow-control]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](pubsub::Subscriber subscriber, pubsub::Subscription const& subscription) {
    std::mutex mu;
    std::condition_variable cv;
    int count = 0;
    auto constexpr kExpectedMessageCount = 4;
    auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
      std::cout << "Received message " << m << "\n";
      std::move(h).ack();
      {
        std::lock_guard<std::mutex> lk(mu);
        if (++count < kExpectedMessageCount) return;
      }
      cv.notify_one();
    };

    // Change the flow control watermarks, by default the client library uses
    // 0 and 1,000 for the message count watermarks, and 0 and 10MiB for the
    // size watermarks. Recall that the library stops requesting messages if
    // any of the high watermarks are reached, and the library resumes
    // requesting messages when *both* low watermarks are reached.
    auto constexpr kMiB = 1024 * 1024L;
    auto session = subscriber.Subscribe(
        subscription, std::move(handler),
        pubsub::SubscriptionOptions{}
            .set_message_count_watermarks(/*lwm=*/800, /*hwm=*/1000)
            .set_message_size_watermarks(/*lwm=*/4 * kMiB, /*hwm=*/8 * kMiB));
    {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [&] { return count >= kExpectedMessageCount; });
    }
    session.cancel();
    auto status = session.get();
    std::cout << "Message count: " << count << ", status: " << status << "\n";
  }
  //! [END pubsub_subscriber_flow_settings] [subscriber-flow-control]
  (std::move(subscriber), std::move(subscription));
}

void SubscriberRetrySettings(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "subscriber-retry-settings <project-id> <subscription-id>"};
  }

  //! [subscriber-retry-settings]
  namespace pubsub = google::cloud::pubsub;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string project_id, std::string subscription_id) {
    // By default a subscriber will retry for 60 seconds, with an initial
    // backoff of 100ms, a maximum backoff of 60 seconds, and the backoff will
    // grow by 30% after each attempt. This changes those defaults.
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::ConnectionOptions{},
        pubsub::LimitedTimeRetryPolicy(
            /*maximum_duration=*/std::chrono::minutes(1))
            .clone(),
        pubsub::ExponentialBackoffPolicy(
            /*initial_delay=*/std::chrono::milliseconds(200),
            /*maximum_delay=*/std::chrono::seconds(10),
            /*scaling=*/2.0)
            .clone()));
    auto subscription =
        pubsub::Subscription(std::move(project_id), std::move(subscription_id));

    std::mutex mu;
    std::condition_variable cv;
    int count = 0;
    auto constexpr kExpectedMessageCount = 1;
    auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
      std::move(h).ack();
      {
        std::lock_guard<std::mutex> lk(mu);
        std::cout << "Received message " << m << "\n";
        if (++count < kExpectedMessageCount) return;
      }
      cv.notify_one();
    };

    auto session = subscriber.Subscribe(subscription, std::move(handler),
                                        pubsub::SubscriptionOptions{});
    {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [&] { return count >= kExpectedMessageCount; });
    }
    session.cancel();
    auto status = session.get();
    std::cout << "Message count: " << count << ", status: " << status << "\n";
  }
  //! [subscriber-retry-settings]
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

  std::cout << "\nRunning CreateTopic() sample [1]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning CreateTopic() sample [2]" << std::endl;
  CreateTopic(topic_admin_client, {project_id, topic_id});

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

  std::cout << "\nRunning ModifyPushConfig() sample" << std::endl;
  ModifyPushConfig(subscription_admin_client,
                   {project_id, push_subscription_id, endpoint2});

  std::cout << "\nRunning DeleteSubscription() sample [1]" << std::endl;
  // Move push_subscription_id to prevent accidentally using it below.
  DeleteSubscription(subscription_admin_client,
                     {project_id, std::move(push_subscription_id)});

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

  std::cout << "\nRunning Publish() sample [1]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning PublishCustomAttributes() sample [1]" << std::endl;
  PublishCustomAttributes(publisher, {});

  std::cout << "\nRunning Subscribe() sample" << std::endl;
  Subscribe(subscriber, subscription, {});

  std::cout << "\nRunning Publish() sample [2]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning SubscribeErrorListener() sample" << std::endl;
  SubscribeErrorListener(subscriber, subscription, {});

  std::cout << "\nRunning the CustomThreadPoolPublisher() sample" << std::endl;
  CustomThreadPoolPublisher({project_id, topic_id});

  std::cout << "\nRunning the PublisherConcurrencyControl() sample"
            << std::endl;
  PublisherConcurrencyControl({project_id, topic_id});

  std::cout << "\nRunning the PublisherRetrySettings() sample" << std::endl;
  PublisherRetrySettings({project_id, topic_id});

  std::cout << "\nRunning the CustomBatchPublisher() sample" << std::endl;
  CustomBatchPublisher({project_id, topic_id});

  std::cout << "\nRunning the CustomThreadPoolSubscriber() sample" << std::endl;
  CustomThreadPoolSubscriber({project_id, subscription_id});

  std::cout << "\nRunning PublishCustomAttributes() sample [2]" << std::endl;
  PublishCustomAttributes(publisher, {});

  std::cout << "\nRunning SubscribeCustomAttributes() sample" << std::endl;
  SubscribeCustomAttributes(subscriber, subscription, {});

  std::cout << "\nRunning Publish() sample [3]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning SubscriberConcurrencyControl() sample" << std::endl;
  SubscriberConcurrencyControl({project_id, subscription_id});

  std::cout << "\nRunning Publish() sample [4]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning SubscriberFlowControlSettings() sample" << std::endl;
  SubscriberFlowControlSettings(subscriber, subscription, {});

  std::cout << "\nRunning Publish() sample [5]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning SubscriberRetrySettings() sample" << std::endl;
  SubscriberRetrySettings({project_id, subscription_id});

  std::cout << "\nRunning DetachSubscription() sample" << std::endl;
  DetachSubscription(topic_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteSubscription() sample [2]" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteSubscription() sample [3]" << std::endl;
  DeleteSubscription(subscription_admin_client, {project_id, subscription_id});

  std::cout << "\nRunning DeleteTopic() sample [1]" << std::endl;
  DeleteTopic(topic_admin_client, {project_id, topic_id});

  std::cout << "\nRunning DeleteTopic() sample [2]" << std::endl;
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
      CreateSubscriptionAdminCommand(
          "seek-with-snapshot",
          {"project-id", "subscription-id", "snapshot-id"}, SeekWithSnapshot),
      CreateSubscriptionAdminCommand(
          "seek-with-timestamp", {"project-id", "subscription-id", "seconds"},
          SeekWithTimestamp),
      CreatePublisherCommand("publish", {}, Publish),
      CreatePublisherCommand("publish-custom-attributes", {},
                             PublishCustomAttributes),
      CreateSubscriberCommand("subscribe", {}, Subscribe),
      CreateSubscriberCommand("subscribe-error-listener", {},
                              SubscribeErrorListener),
      CreateSubscriberCommand("subscribe-custom-attributes", {},
                              SubscribeCustomAttributes),
      {"custom-thread-pool-publisher", CustomThreadPoolPublisher},
      {"publisher-concurrency-control", PublisherConcurrencyControl},
      {"publisher-retry-settings", PublisherRetrySettings},
      {"custom-batch-publisher", CustomBatchPublisher},
      {"custom-thread-pool-subscriber", CustomThreadPoolSubscriber},
      {"subscriber-concurrency-control", SubscriberConcurrencyControl},
      CreateSubscriberCommand("subscriber-flow-control-settings", {},
                              SubscriberFlowControlSettings),
      {"subscriber-retry-settings", SubscriberRetrySettings},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
