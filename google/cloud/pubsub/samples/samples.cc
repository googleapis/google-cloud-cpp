// Copyright 2019 Google LLC
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
#include "google/cloud/pubsub/schema_client.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/project.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/example_driver.h"
#include <google/cloud/pubsub/samples/testdata/schema.pb.h>
#include <google/protobuf/text_format.h>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

namespace {

using ::google::cloud::pubsub::examples::Cleanup;
using ::google::cloud::pubsub::examples::RandomSchemaId;
using ::google::cloud::pubsub::examples::RandomSnapshotId;
using ::google::cloud::pubsub::examples::RandomSubscriptionId;
using ::google::cloud::pubsub::examples::RandomTopicId;
using ::google::cloud::pubsub::examples::ReadFile;
using ::google::cloud::pubsub::examples::UsingEmulator;

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

void ExampleStatusOr(google::cloud::pubsub_admin::TopicAdminClient client,
                     std::vector<std::string> const& argv) {
  //! [example-status-or]
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  [](pubsub_admin::TopicAdminClient client, std::string const& project_id) {
    // The actual type of `topic` is
    // google::cloud::StatusOr<google::pubsub::v1::Topic>, but
    // we expect it'll most often be declared with auto like this.
    for (auto& topic : client.ListTopics(project_id)) {
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

void PublishAvroRecords(google::cloud::pubsub::Publisher publisher,
                        std::vector<std::string> const&) {
  //! [START pubsub_publish_avro_records]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    auto constexpr kNewYork =
        R"js({ "name": "New York", "post_abbr": "NY" })js";
    auto constexpr kPennsylvania =
        R"js({ "name": "Pennsylvania", "post_abbr": "PA" })js";
    std::vector<future<void>> done;
    auto handler = [](future<StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::move(id).status();
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    std::vector<std::pair<std::string, std::string>> states{
        {"New York", "NY"},
        {"Pennsylvania", "PA"},
    };
    std::vector<future<void>> done;
    auto handler = [](future<StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::move(id).status();
    };
    for (auto& data : states) {
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  return [](pubsub::Subscriber subscriber) {
    auto session = subscriber.Subscribe(
        [](pubsub::Message const& m, pubsub::AckHandler h) {
          google::cloud::pubsub::samples::State state;
          state.ParseFromString(std::string{m.data()});
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    auto message_id = publisher.Publish(
        pubsub::MessageBuilder{}.SetData("Hello World!").Build());
    auto done = message_id.then([](future<StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::move(id).status();
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
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
        if (!id) throw std::move(id).status();
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    struct SampleData {
      std::string ordering_key;
      std::string data;
    } data[] = {
        {"key1", "message1"}, {"key2", "message2"}, {"key1", "message3"},
        {"key1", "message4"}, {"key1", "message5"},
    };
    std::vector<future<void>> done;
    for (auto& datum : data) {
      auto message_id =
          publisher.Publish(pubsub::MessageBuilder{}
                                .SetData("Hello World! [" + datum.data + "]")
                                .SetOrderingKey(datum.ordering_key)
                                .Build());
      std::string ack_id = datum.ordering_key + "#" + datum.data;
      done.push_back(message_id.then([ack_id](future<StatusOr<std::string>> f) {
        auto id = f.get();
        if (!id) throw std::move(id).status();
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](pubsub::Publisher publisher) {
    struct SampleData {
      std::string ordering_key;
      std::string data;
    } data[] = {
        {"key1", "message1"}, {"key2", "message2"}, {"key1", "message3"},
        {"key1", "message4"}, {"key1", "message5"},
    };
    std::vector<future<void>> done;
    for (auto& datum : data) {
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
  //! [START pubsub_subscriber_async_pull] [subscribe]
  namespace pubsub = ::google::cloud::pubsub;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message " << m << "\n";
          std::move(h).ack();
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
  };
  //! [END pubsub_subscriber_async_pull] [subscribe]
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) { return count > initial; },
      sample(std::move(subscriber)), __func__);
}

void ExactlyOnceSubscribe(google::cloud::pubsub::Subscriber subscriber,
                          std::vector<std::string> const&) {
  auto const initial = EventCounter::Instance().Current();
  //! [START pubsub_subscriber_exactly_once] [exactly-once-subscribe]
  namespace pubsub = ::google::cloud::pubsub;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::ExactlyOnceAckHandler h) {
          std::cout << "Received message " << m << "\n";
          std::move(h).ack().then([id = m.message_id()](auto f) {
            auto status = f.get();
            std::cout << "Message id " << id
                      << " ack() completed with status=" << status << "\n";
          });
          PleaseIgnoreThisSimplifiesTestingTheSamples();
        });
  };
  //! [END pubsub_subscriber_exactly_once] [exactly-once-subscribe]
  EventCounter::Instance().Wait(
      [initial](std::int64_t count) { return count > initial; },
      sample(std::move(subscriber)), __func__);
}

void Pull(google::cloud::pubsub::Subscriber subscriber,
          std::vector<std::string> const&) {
  //! [START pubsub_subscriber_sync_pull] [pull]
  [](google::cloud::pubsub::Subscriber subscriber) {
    auto response = subscriber.Pull();
    if (!response) throw std::move(response).status();
    std::cout << "Received message " << response->message << "\n";
    std::move(response->handler).ack();
  }
  //! [END pubsub_subscriber_sync_pull] [pull]
  (std::move(subscriber));
}

void SubscribeErrorListener(google::cloud::pubsub::Subscriber subscriber,
                            std::vector<std::string> const&) {
  auto current = EventCounter::Instance().Current();
  // [START pubsub_subscriber_error_listener]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
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

void ReceiveDeadLetterDeliveryAttempt(
    google::cloud::pubsub::Subscriber subscriber,
    std::vector<std::string> const&) {
  auto const initial = EventCounter::Instance().Current();
  //! [dead-letter-delivery-attempt]
  // [START pubsub_dead_letter_delivery_attempt]
  namespace pubsub = ::google::cloud::pubsub;
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

void SubscribeCustomAttributes(google::cloud::pubsub::Subscriber subscriber,
                               std::vector<std::string> const&) {
  auto const initial = EventCounter::Instance().Current();
  //! [START pubsub_subscriber_async_pull_custom_attributes]
  namespace pubsub = ::google::cloud::pubsub;
  auto sample = [](pubsub::Subscriber subscriber) {
    return subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {
          std::cout << "Received message with attributes:\n";
          for (auto& kv : m.attributes()) {
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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::GrpcCompletionQueueOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
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
        std::move(topic), Options{}.set<GrpcCompletionQueueOption>(cq)));

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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::GrpcBackgroundThreadPoolSizeOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // Override the default number of background (I/O) threads. By default the
    // library uses `std::thread::hardware_concurrency()` threads.
    auto options = Options{}.set<GrpcBackgroundThreadPoolSizeOption>(8);
    auto publisher = pubsub::Publisher(
        pubsub::MakePublisherConnection(std::move(topic), std::move(options)));

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

void PublisherFlowControl(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{"publisher-flow-control <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_flow_control]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // Configure the publisher to block if either (1) 100 or more messages, or
    // (2) messages with 100MiB worth of data have not been acknowledged by the
    // service. By default the publisher never blocks, and its capacity is only
    // limited by the system's memory.
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        Options{}
            .set<pubsub::MaxPendingMessagesOption>(100)
            .set<pubsub::MaxPendingBytesOption>(100 * 1024 * 1024L)
            .set<pubsub::FullPublisherActionOption>(
                pubsub::FullPublisherAction::kBlocks)));

    std::vector<future<void>> ids;
    for (char const* data : {"a", "b", "c"}) {
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
  //! [END pubsub_publisher_flow_control]
  (argv.at(0), argv.at(1));
}

void PublisherRetrySettings(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{"publisher-retry-settings <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_retry_settings] [publisher-retry-settings]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // By default a publisher will retry for 60 seconds, with an initial backoff
    // of 100ms, a maximum backoff of 60 seconds, and the backoff will grow by
    // 30% after each attempt. This changes those defaults.
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        Options{}
            .set<pubsub::RetryPolicyOption>(
                pubsub::LimitedTimeRetryPolicy(
                    /*maximum_duration=*/std::chrono::minutes(10))
                    .clone())
            .set<pubsub::BackoffPolicyOption>(
                pubsub::ExponentialBackoffPolicy(
                    /*initial_delay=*/std::chrono::milliseconds(200),
                    /*maximum_delay=*/std::chrono::seconds(45),
                    /*scaling=*/2.0)
                    .clone())));

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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        Options{}
            .set<pubsub::RetryPolicyOption>(
                pubsub::LimitedErrorCountRetryPolicy(/*maximum_failures=*/0)
                    .clone())
            .set<pubsub::BackoffPolicyOption>(
                pubsub::ExponentialBackoffPolicy(
                    /*initial_delay=*/std::chrono::milliseconds(200),
                    /*maximum_delay=*/std::chrono::seconds(45),
                    /*scaling=*/2.0)
                    .clone())));

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

void PublishWithCompression(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{"publish-with-compression <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_with_compression]
  namespace g = ::google::cloud;
  namespace pubsub = ::google::cloud::pubsub;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        g::Options{}
            // Compress any batch of messages over 10 bytes. By default, no
            // messages are compressed, set this to 0 to compress all batches,
            // regardless of their size.
            .set<pubsub::CompressionThresholdOption>(10)
            // Compress using the GZIP algorithm. By default, the library uses
            // GRPC_COMPRESS_DEFLATE.
            .set<pubsub::CompressionAlgorithmOption>(GRPC_COMPRESS_GZIP)));
    auto message_id = publisher.Publish(
        pubsub::MessageBuilder{}.SetData("Hello World!").Build());
    auto done = message_id.then([](g::future<g::StatusOr<std::string>> f) {
      auto id = f.get();
      if (!id) throw std::move(id).status();
      std::cout << "Hello World! published with id=" << *id << "\n";
    });
    // Block until the message is published
    done.get();
  }
  //! [END pubsub_publisher_with_compression]
  (argv.at(0), argv.at(1));
}

void CustomBatchPublisher(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 2) {
    throw examples::Usage{
        "custom-thread-pool-publisher <project-id> <topic-id>"};
  }
  //! [START pubsub_publisher_batch_settings] [publisher-options]
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  [](std::string project_id, std::string topic_id) {
    auto topic = pubsub::Topic(std::move(project_id), std::move(topic_id));
    // By default, the publisher will flush a batch after 10ms, after it
    // contains more than 100 message, or after it contains more than 1MiB of
    // data, whichever comes first. This changes those defaults.
    auto publisher = pubsub::Publisher(pubsub::MakePublisherConnection(
        std::move(topic),
        Options{}
            .set<pubsub::MaxHoldTimeOption>(std::chrono::milliseconds(20))
            .set<pubsub::MaxBatchBytesOption>(4 * 1024 * 1024L)
            .set<pubsub::MaxBatchMessagesOption>(200)));

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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::GrpcCompletionQueueOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
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
        Options{}.set<GrpcCompletionQueueOption>(cq)));

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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::GrpcBackgroundThreadPoolSizeOption;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  auto sample = [](std::string project_id, std::string subscription_id) {
    // Create a subscriber with 16 threads handling I/O work, by default the
    // library creates `std::thread::hardware_concurrency()` threads.
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        Options{}
            .set<pubsub::MaxConcurrencyOption>(8)
            .set<GrpcBackgroundThreadPoolSizeOption>(16)));

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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  auto sample = [](std::string project_id, std::string subscription_id) {
    // Change the flow control watermarks, by default the client library uses
    // 0 and 1,000 for the message count watermarks, and 0 and 10MiB for the
    // size watermarks. Recall that the library stops requesting messages if
    // any of the high watermarks are reached, and the library resumes
    // requesting messages when *both* low watermarks are reached.
    auto constexpr kMiB = 1024 * 1024L;
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        Options{}
            .set<pubsub::MaxOutstandingMessagesOption>(1000)
            .set<pubsub::MaxOutstandingBytesOption>(8 * kMiB)));

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
  namespace pubsub = ::google::cloud::pubsub;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::StatusOr;
  auto sample = [](std::string project_id, std::string subscription_id) {
    // By default a subscriber will retry for 60 seconds, with an initial
    // backoff of 100ms, a maximum backoff of 60 seconds, and the backoff will
    // grow by 30% after each attempt. This changes those defaults.
    auto subscriber = pubsub::Subscriber(pubsub::MakeSubscriberConnection(
        pubsub::Subscription(std::move(project_id), std::move(subscription_id)),
        Options{}
            .set<pubsub::RetryPolicyOption>(
                pubsub::LimitedTimeRetryPolicy(
                    /*maximum_duration=*/std::chrono::minutes(1))
                    .clone())
            .set<pubsub::BackoffPolicyOption>(
                pubsub::ExponentialBackoffPolicy(
                    /*initial_delay=*/std::chrono::milliseconds(200),
                    /*maximum_delay=*/std::chrono::seconds(10),
                    /*scaling=*/2.0)
                    .clone())));

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

void OptimisticSubscribe(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;
  if (argv.size() != 3) {
    throw examples::Usage{
        "optimistic-subscribe <project-id> <topic-id> <subscription-id>"};
  }
  namespace pubsub_admin = ::google::cloud::pubsub_admin;
  namespace pubsub = ::google::cloud::pubsub;
  namespace gc = ::google::cloud;
  [](std::string project_id, std::string topic_id,
     std::string subscription_id) {
    // Do not retry the attempts to consume messages.
    auto subscriber = pubsub::Subscriber(
        pubsub::MakeSubscriberConnection(
            pubsub::Subscription(project_id, subscription_id)),
        google::cloud::Options{}.set<pubsub::RetryPolicyOption>(
            pubsub::LimitedErrorCountRetryPolicy(
                /*maximum_failures=*/0)
                .clone()));

    // [START pubsub_optimistic_subscribe]
    auto process_response = [](gc::StatusOr<pubsub::PullResponse> response) {
      if (response) {
        std::cout << "Received message " << response->message << "\n";
        std::move(response->handler).ack();
        return gc::Status();
      }
      if (response.status().code() == gc::StatusCode::kUnavailable &&
          response.status().message() == "no messages returned") {
        std::cout << "No messages returned from Pull()\n";
        return gc::Status();
      }
      return response.status();
    };

    // Instead of checking if the subscription exists, optimistically try to
    // consume from the subscription.
    auto status = process_response(subscriber.Pull());
    if (status.ok()) return;
    if (status.code() != gc::StatusCode::kNotFound) throw std::move(status);

    // Since the subscription does not exist, create the subscription.
    pubsub_admin::SubscriptionAdminClient subscription_admin_client(
        pubsub_admin::MakeSubscriptionAdminConnection());
    google::pubsub::v1::Subscription request;
    request.set_name(
        pubsub::Subscription(project_id, subscription_id).FullName());
    request.set_topic(
        pubsub::Topic(project_id, std::move(topic_id)).FullName());
    auto sub = subscription_admin_client.CreateSubscription(request);
    if (!sub) throw std::move(sub).status();

    // Consume from the new subscription.
    status = process_response(subscriber.Pull());
    if (!status.ok()) throw std::move(status);
    // [END pubsub_optimistic_subscribe]
  }(argv.at(0), argv.at(1), argv.at(2));
}

void AutoRunAvro(
    std::string const& project_id, std::string const& testdata_directory,
    google::cloud::internal::DefaultPRNG& generator,
    google::cloud::pubsub_admin::TopicAdminClient& topic_admin,
    google::cloud::pubsub_admin::SubscriptionAdminClient& subscription_admin) {
  auto schema_admin = google::cloud::pubsub::SchemaServiceClient(
      google::cloud::pubsub::MakeSchemaServiceConnection());
  auto avro_schema_id = RandomSchemaId(generator);
  auto avro_schema_definition_file = testdata_directory + "schema.avsc";
  auto schema = google::cloud::pubsub::Schema(project_id, avro_schema_id);

  std::cout << "\nCreating test schema [avro]" << std::endl;
  google::pubsub::v1::CreateSchemaRequest schema_request;
  schema_request.set_parent(google::cloud::Project(project_id).FullName());
  schema_request.set_schema_id(avro_schema_id);
  schema_request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
  std::string const definition = ReadFile(avro_schema_definition_file);
  schema_request.mutable_schema()->set_definition(definition);
  auto schema_result = schema_admin.CreateSchema(schema_request);
  if (!schema_result) throw std::move(schema_result).status();

  Cleanup cleanup;
  cleanup.Defer([schema_admin, schema]() mutable {
    std::cout << "\nDelete schema (" << schema.schema_id() << ") [avro]"
              << std::endl;
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(schema.FullName());
    schema_admin.DeleteSchema(request);
  });

  std::cout << "\nCreating test topic with schema [avro]" << std::endl;
  auto const avro_topic_id = RandomTopicId(generator);
  auto topic = google::cloud::pubsub::Topic(project_id, avro_topic_id);
  google::pubsub::v1::Topic topic_request;
  topic_request.set_name(topic.FullName());
  topic_request.mutable_schema_settings()->set_schema(schema.FullName());
  topic_request.mutable_schema_settings()->set_encoding(
      google::pubsub::v1::JSON);
  auto t = topic_admin.CreateTopic(topic_request);
  if (!t) {
    std::cerr << t.status() << "\n";
  }
  cleanup.Defer([topic_admin, topic]() mutable {
    std::cout << "\nDelete topic (" << topic.topic_id() << ") [avro]"
              << std::endl;
    (void)topic_admin.DeleteTopic(topic.FullName());
  });

  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::Options{}
                     .set<google::cloud::pubsub::MaxBatchMessagesOption>(1)));

  auto const subscription_id = RandomSubscriptionId(generator);
  auto subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(subscription));

  std::cout << "\nCreate test subscription (" << subscription.subscription_id()
            << ") [avro]" << std::endl;
  google::pubsub::v1::Subscription request;
  request.set_name(subscription.FullName());
  request.set_topic(topic.FullName());
  (void)subscription_admin.CreateSubscription(request);
  cleanup.Defer([subscription_admin, subscription]() mutable {
    std::cout << "\nDelete subscription (" << subscription.subscription_id()
              << ") [avro]" << std::endl;
    (void)subscription_admin.DeleteSubscription(subscription.FullName());
  });

  std::cout << "\nRunning SubscribeAvroRecords() sample" << std::endl;
  auto session = SubscribeAvroRecords(subscriber, {});

  std::cout << "\nRunning PublishAvroRecords() sample" << std::endl;
  PublishAvroRecords(publisher, {});

  session.cancel();
  WaitForSession(std::move(session), "SubscribeAvroRecords");
}

void AutoRunProtobuf(
    std::string const& project_id, std::string const& testdata_directory,
    google::cloud::internal::DefaultPRNG& generator,
    google::cloud::pubsub_admin::TopicAdminClient& topic_admin,
    google::cloud::pubsub_admin::SubscriptionAdminClient& subscription_admin) {
  auto schema_admin = google::cloud::pubsub::SchemaServiceClient(
      google::cloud::pubsub::MakeSchemaServiceConnection());
  auto proto_schema_id = RandomSchemaId(generator);
  auto proto_schema_definition_file = testdata_directory + "schema.proto";
  auto schema = google::cloud::pubsub::Schema(project_id, proto_schema_id);

  std::cout << "\nCreating test schema [proto]" << std::endl;
  google::pubsub::v1::CreateSchemaRequest schema_request;
  schema_request.set_parent(google::cloud::Project(project_id).FullName());
  schema_request.set_schema_id(proto_schema_id);
  schema_request.mutable_schema()->set_type(
      google::pubsub::v1::Schema::PROTOCOL_BUFFER);
  std::string const definition = ReadFile(proto_schema_definition_file);
  schema_request.mutable_schema()->set_definition(definition);
  auto schema_result = schema_admin.CreateSchema(schema_request);
  if (!schema_result) throw std::move(schema_result).status();

  Cleanup cleanup;
  cleanup.Defer([schema_admin, schema]() mutable {
    std::cout << "\nDelete schema (" << schema.schema_id() << ") [proto]"
              << std::endl;
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(schema.FullName());
    schema_admin.DeleteSchema(request);
  });

  std::cout << "\nCreating test topic with schema [proto]" << std::endl;
  auto const proto_topic_id = RandomTopicId(generator);
  auto topic = google::cloud::pubsub::Topic(project_id, proto_topic_id);
  google::pubsub::v1::Topic topic_request;
  topic_request.set_name(topic.FullName());
  topic_request.mutable_schema_settings()->set_schema(schema.FullName());
  topic_request.mutable_schema_settings()->set_encoding(
      google::pubsub::v1::BINARY);
  topic_admin.CreateTopic(topic_request);
  cleanup.Defer([topic_admin, topic]() mutable {
    std::cout << "\nDelete topic (" << topic.topic_id() << ") [proto]"
              << std::endl;
    (void)topic_admin.DeleteTopic(topic.FullName());
  });

  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::Options{}
                     .set<google::cloud::pubsub::MaxBatchMessagesOption>(1)));

  auto const subscription_id = RandomSubscriptionId(generator);
  auto subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(subscription));

  std::cout << "\nCreate test subscription (" << subscription.subscription_id()
            << ") [proto]" << std::endl;
  google::pubsub::v1::Subscription request;
  request.set_name(subscription.FullName());
  request.set_topic(topic.FullName());
  (void)subscription_admin.CreateSubscription(request);
  cleanup.Defer([subscription_admin, subscription]() mutable {
    std::cout << "\nDelete subscription (" << subscription.subscription_id()
              << ") [proto]" << std::endl;
    (void)subscription_admin.DeleteSubscription(subscription.FullName());
  });

  std::cout << "\nRunning SubscribeProtobufRecords() sample" << std::endl;
  auto session = SubscribeProtobufRecords(subscriber, {});

  std::cout << "\nRunning PublishProtobufRecords() sample" << std::endl;
  PublishProtobufRecords(publisher, {});

  session.cancel();
  WaitForSession(std::move(session), "SubscribeProtobufRecords");
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::testing_util;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();

  // For CMake builds, use the environment variable. For Bazel builds, use the
  // relative path to the file.
  auto const testdata_directory =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_PUBSUB_TESTDATA")
          .value_or("./google/cloud/pubsub/samples/testdata/");

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const topic_id = RandomTopicId(generator);
  auto const topic = google::cloud::pubsub::Topic(project_id, topic_id);
  auto const subscription_id = RandomSubscriptionId(generator);
  auto const subscription =
      google::cloud::pubsub::Subscription(project_id, subscription_id);
  auto const exactly_once_subscription_id = RandomSubscriptionId(generator);
  auto const exactly_once_subscription = google::cloud::pubsub::Subscription(
      project_id, exactly_once_subscription_id);
  auto const filtered_subscription_id = RandomSubscriptionId(generator);
  auto const filtered_subscription =
      google::cloud::pubsub::Subscription(project_id, filtered_subscription_id);
  auto const ordering_subscription_id = RandomSubscriptionId(generator);
  auto const ordering_subscription =
      google::cloud::pubsub::Subscription(project_id, ordering_subscription_id);
  auto const ordering_topic_id = "ordering-" + RandomTopicId(generator);
  auto const ordering_topic =
      google::cloud::pubsub::Topic(project_id, ordering_topic_id);
  auto const dead_letter_subscription_id = RandomSubscriptionId(generator);
  auto const dead_letter_subscription = google::cloud::pubsub::Subscription(
      project_id, dead_letter_subscription_id);
  auto const dead_letter_topic_id = "dead-letter-" + RandomTopicId(generator);
  auto const dead_letter_topic =
      google::cloud::pubsub::Topic(project_id, dead_letter_topic_id);
  auto const snapshot_id = RandomSnapshotId(generator);

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

  google::cloud::pubsub_admin::TopicAdminClient topic_admin(
      google::cloud::pubsub_admin::MakeTopicAdminConnection());
  google::cloud::pubsub_admin::SubscriptionAdminClient subscription_admin(
      google::cloud::pubsub_admin::MakeSubscriptionAdminConnection());

  std::cout << "\nCreate topic (" << topic.topic_id() << ")" << std::endl;
  topic_admin.CreateTopic(topic.FullName());
  std::cout << "\nCreate topic (" << ordering_topic.topic_id() << ")"
            << std::endl;
  topic_admin.CreateTopic(ordering_topic.FullName());
  std::cout << "\nCreate topic (" << dead_letter_topic.topic_id() << ")"
            << std::endl;
  topic_admin.CreateTopic(dead_letter_topic.FullName());
  Cleanup cleanup;
  cleanup.Defer(
      [topic_admin, topic, ordering_topic, dead_letter_topic]() mutable {
        std::cout << "\nDelete topic (" << topic.topic_id() << ")" << std::endl;
        (void)topic_admin.DeleteTopic(topic.FullName());
        std::cout << "\nDelete topic (" << ordering_topic.topic_id() << ")"
                  << std::endl;
        (void)topic_admin.DeleteTopic(ordering_topic.FullName());
        std::cout << "\nDelete topic (" << dead_letter_topic.topic_id() << ")"
                  << std::endl;
        (void)topic_admin.DeleteTopic(dead_letter_topic.FullName());
      });

  std::cout << "\nRunning the StatusOr example" << std::endl;
  ExampleStatusOr(topic_admin, {project_id});

  std::cout << "\nCreate subscription (" << subscription.subscription_id()
            << ")" << std::endl;
  google::pubsub::v1::Subscription request;
  request.set_name(subscription.FullName());
  request.set_topic(topic.FullName());
  (void)subscription_admin.CreateSubscription(request);
  std::cout << "\nCreate filtered subscription ("
            << filtered_subscription.subscription_id() << ")" << std::endl;
  google::pubsub::v1::Subscription filtered_request;
  filtered_request.set_name(filtered_subscription.FullName());
  filtered_request.set_topic(topic.FullName());
  filtered_request.set_filter(R"""(attributes.is-even = "false")""");
  (void)subscription_admin.CreateSubscription(filtered_request);
  std::cout << "\nCreate exactly once subscription ("
            << exactly_once_subscription.subscription_id() << ")" << std::endl;
  google::pubsub::v1::Subscription exactly_once_request;
  exactly_once_request.set_name(exactly_once_subscription.FullName());
  exactly_once_request.set_topic(topic.FullName());
  exactly_once_request.set_enable_exactly_once_delivery(true);
  (void)subscription_admin.CreateSubscription(exactly_once_request);
  std::cout << "\nCreate ordering subscription ("
            << ordering_subscription.subscription_id() << ")" << std::endl;
  google::pubsub::v1::Subscription ordering_request;
  ordering_request.set_name(ordering_subscription.FullName());
  ordering_request.set_topic(topic.FullName());
  ordering_request.set_enable_message_ordering(true);
  (void)subscription_admin.CreateSubscription(ordering_request);
  std::cout << "\nCreate dead letter subscription ("
            << dead_letter_subscription.subscription_id() << ")" << std::endl;
  // Hardcode this number as it does not really matter. The other samples
  // pick something between 10 and 15.
  auto constexpr kDeadLetterDeliveryAttempts = 15;
  google::pubsub::v1::Subscription dead_letter_request;
  dead_letter_request.set_name(dead_letter_subscription.FullName());
  dead_letter_request.set_topic(dead_letter_topic.FullName());
  dead_letter_request.mutable_dead_letter_policy()->set_dead_letter_topic(
      dead_letter_topic.FullName());
  dead_letter_request.mutable_dead_letter_policy()->set_max_delivery_attempts(
      kDeadLetterDeliveryAttempts);
  (void)subscription_admin.CreateSubscription(dead_letter_request);
  cleanup.Defer([subscription_admin, subscription, filtered_subscription,
                 exactly_once_subscription, ordering_subscription,
                 dead_letter_subscription]() mutable {
    std::cout << "\nDelete subscription (" << subscription.subscription_id()
              << ")" << std::endl;
    (void)subscription_admin.DeleteSubscription(subscription.FullName());
    std::cout << "\nDelete subscription ("
              << filtered_subscription.subscription_id() << ")" << std::endl;
    (void)subscription_admin.DeleteSubscription(
        filtered_subscription.FullName());
    std::cout << "\nDelete subscription ("
              << exactly_once_subscription.subscription_id() << ")"
              << std::endl;
    (void)subscription_admin.DeleteSubscription(
        exactly_once_subscription.FullName());
    std::cout << "\nDelete subscription ("
              << ordering_subscription.subscription_id() << ")" << std::endl;
    (void)subscription_admin.DeleteSubscription(
        ordering_subscription.FullName());
    std::cout << "\nDelete subscription ("
              << dead_letter_subscription.subscription_id() << ")" << std::endl;
    (void)subscription_admin.DeleteSubscription(
        dead_letter_subscription.FullName());
  });

  ignore_emulator_failures([&] {
    AutoRunAvro(project_id, testdata_directory, generator, topic_admin,
                subscription_admin);
  });
  ignore_emulator_failures([&] {
    AutoRunProtobuf(project_id, testdata_directory, generator, topic_admin,
                    subscription_admin);
  });

  auto publisher = google::cloud::pubsub::Publisher(
      google::cloud::pubsub::MakePublisherConnection(
          topic, google::cloud::Options{}
                     .set<google::cloud::pubsub::MaxBatchMessagesOption>(1)));
  auto subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(subscription));

  auto dead_letter_subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(
          dead_letter_subscription));

  auto exactly_once_subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(
          google::cloud::pubsub::Subscription(project_id,
                                              exactly_once_subscription_id)));

  auto filtered_subscriber = google::cloud::pubsub::Subscriber(
      google::cloud::pubsub::MakeSubscriberConnection(filtered_subscription));

  std::cout << "\nRunning Publish() sample [1]" << std::endl;
  Publish(publisher, {});

  std::cout << "\nRunning PublishCustomAttributes() sample [1]" << std::endl;
  PublishCustomAttributes(publisher, {});

  std::cout << "\nRunning Subscribe() sample" << std::endl;
  Subscribe(subscriber, {});

  std::cout << "\nRunning ExactlyOnceSubscribe() sample" << std::endl;
  ExactlyOnceSubscribe(exactly_once_subscriber, {});

  std::cout << "\nRunning Pull() sample" << std::endl;
  PublishHelper(publisher, "Pull()", 1);
  Pull(subscriber, {});

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

  std::cout << "\nRunning the CustomThreadPoolPublisher() sample" << std::endl;
  CustomThreadPoolPublisher({project_id, topic_id});

  std::cout << "\nRunning the PublisherConcurrencyControl() sample"
            << std::endl;
  PublisherConcurrencyControl({project_id, topic_id});

  std::cout << "\nRunning the PublisherFlowControl() sample" << std::endl;
  PublisherFlowControl({project_id, topic_id});

  std::cout << "\nRunning the PublisherRetrySettings() sample" << std::endl;
  PublisherRetrySettings({project_id, topic_id});

  std::cout << "\nRunning the PublisherDisableRetries() sample" << std::endl;
  PublisherDisableRetries({project_id, topic_id});

  std::cout << "\nRunning the PublishWithCompression() sample" << std::endl;
  PublishWithCompression({project_id, topic_id});

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
          topic, google::cloud::Options{}
                     .set<google::cloud::pubsub::MaxBatchMessagesOption>(1)
                     .set<google::cloud::pubsub::MessageOrderingOption>(true)));
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

  std::cout << "\nRunning OptimisticSubscribe() sample" << std::endl;
  PublishHelper(publisher, "OptimisticSubscribe", 1);
  OptimisticSubscribe({project_id, topic_id, subscription_id});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using ::google::cloud::pubsub::examples::CreatePublisherCommand;
  using ::google::cloud::pubsub::examples::CreateSubscriberCommand;
  using ::google::cloud::testing_util::Example;

  Example example({
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
      CreateSubscriberCommand("exactly-once-subscribe", {},
                              ExactlyOnceSubscribe),
      CreateSubscriberCommand("pull", {}, Pull),
      CreateSubscriberCommand("subscribe-error-listener", {},
                              SubscribeErrorListener),
      CreateSubscriberCommand("subscribe-custom-attributes", {},
                              SubscribeCustomAttributes),
      CreateSubscriberCommand("receive-dead-letter-delivery-attempt", {},
                              ReceiveDeadLetterDeliveryAttempt),
      {"custom-thread-pool-publisher", CustomThreadPoolPublisher},
      {"publisher-concurrency-control", PublisherConcurrencyControl},
      {"publisher-flow-control", PublisherFlowControl},
      {"publisher-retry-settings", PublisherRetrySettings},
      {"publisher-disable-retry", PublisherDisableRetries},
      {"publish-with-compression", PublishWithCompression},
      {"custom-batch-publisher", CustomBatchPublisher},
      {"custom-thread-pool-subscriber", CustomThreadPoolSubscriber},
      {"subscriber-concurrency-control", SubscriberConcurrencyControl},
      {"subscriber-flow-control-settings", SubscriberFlowControlSettings},
      {"subscriber-retry-settings", SubscriberRetrySettings},
      {"optimistic-subscribe", OptimisticSubscribe},
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
