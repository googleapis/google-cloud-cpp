// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/admin/subscription_admin_client.h"
#include "google/cloud/pubsub/admin/topic_admin_client.h"
#include "google/cloud/pubsub/internal/batch_callback.h"
#include "google/cloud/pubsub/internal/default_batch_callback.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/message_callback.h"
#include "google/cloud/pubsub/internal/noop_message_callback.h"
#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/internal/subscriber_stub_factory.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/status_utils.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <set>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub_admin::MakeSubscriptionAdminConnection;
using ::google::cloud::pubsub_admin::MakeTopicAdminConnection;
using ::google::cloud::pubsub_admin::SubscriptionAdminClient;
using ::google::cloud::pubsub_admin::TopicAdminClient;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::NotNull;

class SubscriberIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    auto project_id =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());
    generator_ = google::cloud::internal::DefaultPRNG(std::random_device{}());
    topic_ = Topic(project_id, pubsub_testing::RandomTopicId(generator_));
    subscription_ = Subscription(
        project_id, pubsub_testing::RandomSubscriptionId(generator_));
    ordered_subscription_ = Subscription(
        project_id, pubsub_testing::RandomSubscriptionId(generator_));
    exactly_once_subscription_ = Subscription(
        project_id, pubsub_testing::RandomSubscriptionId(generator_));

    auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
    auto subscription_admin =
        SubscriptionAdminClient(MakeSubscriptionAdminConnection());

    auto topic_metadata = topic_admin.CreateTopic(topic_.FullName());
    ASSERT_THAT(topic_metadata,
                AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

    google::pubsub::v1::Subscription request;
    request.set_name(subscription_.FullName());
    request.set_topic(topic_.FullName());
    request.set_ack_deadline_seconds(10);
    auto subscription_metadata = subscription_admin.CreateSubscription(request);
    ASSERT_THAT(subscription_metadata,
                AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

    google::pubsub::v1::Subscription ordered_request;
    ordered_request.set_name(ordered_subscription_.FullName());
    ordered_request.set_topic(topic_.FullName());
    ordered_request.set_ack_deadline_seconds(30);
    ordered_request.set_enable_message_ordering(true);
    auto ordered_subscription_metadata =
        subscription_admin.CreateSubscription(ordered_request);
    ASSERT_THAT(ordered_subscription_metadata,
                AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));

    google::pubsub::v1::Subscription exactly_once_request;
    exactly_once_request.set_topic(topic_.FullName());
    exactly_once_request.set_name(exactly_once_subscription_.FullName());
    exactly_once_request.set_ack_deadline_seconds(30);
    exactly_once_request.set_enable_exactly_once_delivery(true);
    auto exactly_once_subscription_metadata =
        subscription_admin.CreateSubscription(exactly_once_request);
    ASSERT_THAT(exactly_once_subscription_metadata,
                AnyOf(IsOk(), StatusIs(StatusCode::kAlreadyExists)));
  }

  void TearDown() override {
    auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
    auto subscription_admin =
        SubscriptionAdminClient(MakeSubscriptionAdminConnection());

    auto delete_exactly_once_subscription =
        subscription_admin.DeleteSubscription(
            exactly_once_subscription_.FullName());
    EXPECT_THAT(delete_exactly_once_subscription,
                AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));
    auto delete_ordered_subscription =
        subscription_admin.DeleteSubscription(ordered_subscription_.FullName());
    EXPECT_THAT(delete_ordered_subscription,
                AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));
    auto delete_subscription =
        subscription_admin.DeleteSubscription(subscription_.FullName());
    EXPECT_THAT(delete_subscription,
                AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));
    auto delete_topic = topic_admin.DeleteTopic(topic_.FullName());
    EXPECT_THAT(delete_topic, AnyOf(IsOk(), StatusIs(StatusCode::kNotFound)));
  }

  google::cloud::internal::DefaultPRNG generator_;
  Topic topic_ = Topic("unused", "unused");
  Subscription subscription_ = Subscription("unused", "unused");
  Subscription ordered_subscription_ = Subscription("unused", "unused");
  Subscription exactly_once_subscription_ = Subscription("unused", "unused");
};

void TestRoundtrip(pubsub::Publisher publisher, pubsub::Subscriber subscriber) {
  std::mutex mu;
  std::map<std::string, int> ids;
  for (auto const* data : {"message-0", "message-1", "message-2"}) {
    auto response =
        publisher.Publish(MessageBuilder{}.SetData(data).Build()).get();
    EXPECT_STATUS_OK(response);
    if (response) {
      std::lock_guard<std::mutex> lk(mu);
      ids.emplace(*std::move(response), 0);
    }
  }
  EXPECT_FALSE(ids.empty());

  promise<void> ids_empty;
  auto handler = [&](pubsub::Message const& m, AckHandler h) {
    SCOPED_TRACE("Search for message " + m.message_id());
    std::unique_lock<std::mutex> lk(mu);
    auto i = ids.find(m.message_id());
    // Remember that Cloud Pub/Sub has "at least once" semantics, so a dup is
    // perfectly possible, in that case the message would not be in the map of
    // of pending ids.
    if (i == ids.end()) return;
    // The first time just NACK the message to exercise that path, we expect
    // Cloud Pub/Sub to retry.
    if (i->second == 0) {
      std::move(h).nack();
      ++i->second;
      return;
    }
    ids.erase(i);
    if (ids.empty()) ids_empty.set_value();
    lk.unlock();
    std::move(h).ack();
  };

  auto result = subscriber.Subscribe(handler);
  // Wait until there are no more ids pending, then cancel the subscription and
  // get its status.
  ids_empty.get_future().get();
  result.cancel();
  EXPECT_STATUS_OK(result.get());
}

TEST_F(SubscriberIntegrationTest, Stub) {
  auto publisher = Publisher(MakePublisherConnection(topic_));

  internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto stub = pubsub_internal::MakeRoundRobinSubscriberStub(
      background.cq(), pubsub_internal::DefaultCommonOptions({}));
  google::pubsub::v1::StreamingPullRequest request;
  request.set_client_id("test-client-0001");
  request.set_subscription(subscription_.FullName());
  request.set_max_outstanding_messages(1000);
  request.set_stream_ack_deadline_seconds(600);

  auto stream = [&stub](CompletionQueue const& cq) {
    return stub->AsyncStreamingPull(
        cq, std::make_shared<grpc::ClientContext>(),
        google::cloud::internal::MakeImmutableOptions({}));
  }(background.cq());

  ASSERT_TRUE(stream->Start().get());
  ASSERT_TRUE(
      stream->Write(request, grpc::WriteOptions{}.set_write_through()).get());

  auto constexpr kPublishCount = 1000;
  std::set<std::string> expected_ids = [&] {
    std::set<std::string> ids;
    std::vector<future<StatusOr<std::string>>> message_ids;
    for (int i = 0; i != kPublishCount; ++i) {
      message_ids.push_back(publisher.Publish(
          MessageBuilder{}.SetData("message-" + std::to_string(i)).Build()));
    }
    for (auto& id : message_ids) {
      auto r = id.get();
      EXPECT_STATUS_OK(r);
      if (r) ids.insert(*std::move(r));
    }
    return ids;
  }();

  for (auto r = stream->Read().get(); r.has_value(); r = stream->Read().get()) {
    google::pubsub::v1::StreamingPullRequest acks;
    for (auto const& m : r->received_messages()) {
      acks.add_ack_ids(m.ack_id());
      expected_ids.erase(m.message().message_id());
    }
    auto write_ok = stream->Write(acks, grpc::WriteOptions{}).get();
    if (!write_ok) break;
    if (expected_ids.empty()) break;
  }
  EXPECT_TRUE(expected_ids.empty());

  stream->Cancel();
  // Before closing the stream we need to wait for:
  //     Read().get().has_value() == false
  for (auto r = stream->Read().get(); r.has_value(); r = stream->Read().get()) {
  }

  EXPECT_THAT(stream->Finish().get(),
              AnyOf(IsOk(), StatusIs(StatusCode::kCancelled)));
}

TEST_F(SubscriberIntegrationTest, StreamingSubscriptionBatchSource) {
  // Declare these before any helpers that launch threads. Their lifetime must
  // be longer than any thread pools created by the test, because they are used
  // by those threads.
  //
  // Under heavy load (such as we experience in the CI builds) the main thread
  // would call the destructor for these objects before the threads are done
  // with them.
  std::mutex callback_mu;
  std::condition_variable callback_cv;
  std::set<std::string> received_ids;
  int ack_count = 0;
  int callback_count = 0;
  auto wait_received_count = [&](std::size_t count) {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return received_ids.size() >= count; });
  };

  auto publisher = Publisher(MakePublisherConnection(
      topic_, Options{}.set<GrpcBackgroundThreadPoolSizeOption>(2)));

  internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto stub = pubsub_internal::MakeRoundRobinSubscriberStub(
      background.cq(), pubsub_internal::DefaultCommonOptions({}));

  auto shutdown = std::make_shared<pubsub_internal::SessionShutdownManager>();
  auto source =
      std::make_shared<pubsub_internal::StreamingSubscriptionBatchSource>(
          background.cq(), shutdown, stub, subscription_.FullName(),
          "test-client-0001",
          pubsub_internal::DefaultSubscriberOptions(
              pubsub_testing::MakeTestOptions(
                  Options{}.set<MaxDeadlineTimeOption>(
                      std::chrono::seconds(300)))));

  // This must be declared after `source` as it captures it and uses it to send
  // back acknowledgements.
  std::shared_ptr<pubsub_internal::BatchCallback> batch_callback =
      std::make_shared<pubsub_internal::DefaultBatchCallback>(
          [&](pubsub_internal::BatchCallback::StreamingPullResponse r) {
            ASSERT_STATUS_OK(r.response);
            {
              std::lock_guard<std::mutex> lk(callback_mu);
              for (auto const& m : r.response->received_messages()) {
                received_ids.insert(m.message().message_id());
              }
              ++callback_count;
              for (auto const& m : r.response->received_messages()) {
                source->AckMessage(m.ack_id());
              }
              ack_count += r.response->received_messages_size();
              std::cout << "callback(" << r.response->received_messages_size()
                        << ")" << ", ack_count=" << ack_count
                        << ", received_ids.size()=" << received_ids.size()
                        << std::endl;
            }
            // This condition variable must have a lifetime longer than the
            // thread pools.
            callback_cv.notify_one();
          },
          std::make_shared<pubsub_internal::NoopMessageCallback>());

  auto done = shutdown->Start({});
  source->Start(batch_callback);

  auto constexpr kPublishCount = 1000;
  auto const expected_ids = [&] {
    std::vector<future<StatusOr<std::string>>> message_ids;
    for (int i = 0; i != kPublishCount; ++i) {
      message_ids.push_back(publisher.Publish(
          MessageBuilder{}.SetData("message-" + std::to_string(i)).Build()));
    }
    std::set<std::string> ids;
    for (auto& id : message_ids) {
      auto r = id.get();
      EXPECT_STATUS_OK(r);
      if (r) ids.insert(*std::move(r));
    }
    return ids;
  }();

  wait_received_count(expected_ids.size());

  // Wait until all the background callbacks complete.
  shutdown->MarkAsShutdown("test", {});
  source->Shutdown();

  EXPECT_STATUS_OK(done.get());

  auto diff = [&] {
    // No locks needed as the background threads have stopped.
    std::vector<std::string> diff;
    std::set_symmetric_difference(received_ids.begin(), received_ids.end(),
                                  expected_ids.begin(), expected_ids.end(),
                                  std::back_inserter(diff));
    return diff;
  }();
  EXPECT_THAT(diff, IsEmpty());
}

TEST_F(SubscriberIntegrationTest, PublishPullAck) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber = Subscriber(MakeSubscriberConnection(subscription_));
  ASSERT_NO_FATAL_FAILURE(TestRoundtrip(publisher, subscriber));
}

TEST_F(SubscriberIntegrationTest, FireAndForget) {
  std::mutex mu;
  std::condition_variable cv;
  std::set<std::string> received;
  Status subscription_result;
  std::set<std::string> published;
  std::vector<Status> publish_errors;
  auto constexpr kMinimumMessages = 10;

  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber = Subscriber(MakeSubscriberConnection(subscription_));
  {
    (void)subscriber
        .Subscribe([&](Message const& m, AckHandler h) {
          std::move(h).ack();
          std::unique_lock<std::mutex> lk(mu);
          std::cout << "received " << m.message_id() << std::endl;
          received.insert(m.message_id());
          lk.unlock();
          cv.notify_one();
        })
        .then([&](future<Status> f) {
          std::unique_lock<std::mutex> lk(mu);
          subscription_result = f.get();
          cv.notify_one();
        });

    std::vector<future<void>> pending;
    for (int i = 0; i != kMinimumMessages; ++i) {
      pending.push_back(
          publisher
              .Publish(MessageBuilder{}
                           .SetAttributes({{"index", std::to_string(i)}})
                           .Build())
              .then([&](future<StatusOr<std::string>> f) {
                std::unique_lock<std::mutex> lk(mu);
                auto s = f.get();
                if (!s) {
                  publish_errors.push_back(std::move(s).status());
                  return;
                }
                published.insert(*std::move(s));
              }));
    }
    publisher.Flush();
    for (auto& p : pending) p.get();
  }
  {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return received.size() >= published.size(); });
  }
  EXPECT_THAT(publish_errors, IsEmpty());
  EXPECT_THAT(received, ElementsAreArray(published));
}

TEST_F(SubscriberIntegrationTest, ReportNotFound) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto const not_found_id = pubsub_testing::RandomSubscriptionId(generator_);
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto const subscription = pubsub::Subscription(project_id, not_found_id);
  auto subscriber = Subscriber(MakeSubscriberConnection(subscription));

  auto handler = [](pubsub::Message const&, AckHandler h) {
    std::move(h).ack();
  };

  auto result = subscriber.Subscribe(handler);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kNotFound));
}

TEST_F(SubscriberIntegrationTest, PublishOrdered) {
  auto publisher = Publisher(MakePublisherConnection(
      topic_, Options{}.set<MessageOrderingOption>(true)));
  auto subscriber = Subscriber(MakeSubscriberConnection(ordered_subscription_));

  struct SampleData {
    std::string ordering_key;
    std::string data;
  } data[] = {
      {"key1", "message1-1"}, {"key2", "message2-1"}, {"key1", "message1-2"},
      {"key1", "message1-3"}, {"key2", "message2-2"},
  };

  std::mutex mu;
  std::map<std::string, int> ids;
  std::vector<future<void>> responses;
  for (auto const& d : data) {
    responses.push_back(publisher
                            .Publish(MessageBuilder{}
                                         .SetData(d.data)
                                         .SetOrderingKey(d.ordering_key)
                                         .Build())
                            .then([&](future<StatusOr<std::string>> f) {
                              auto id = f.get();
                              if (!id) return;
                              std::unique_lock<std::mutex> lk(mu);
                              ids.emplace(*id, 0);
                            }));
    publisher.ResumePublish("key2");
  }
  publisher.Flush();
  for (auto& f : responses) f.get();
  EXPECT_FALSE(ids.empty());

  promise<void> ids_empty;
  auto handler = [&](pubsub::Message const& m, AckHandler h) {
    SCOPED_TRACE("Search for message " + m.message_id());
    std::unique_lock<std::mutex> lk(mu);
    auto i = ids.find(m.message_id());
    if (i == ids.end()) return;
    ids.erase(i);
    if (ids.empty()) ids_empty.set_value();
    lk.unlock();
    std::move(h).ack();
  };

  auto result = subscriber.Subscribe(handler);
  // Wait until there are no more ids pending, then cancel the subscription and
  // get its status.
  ids_empty.get_future().get();
  result.cancel();
  EXPECT_STATUS_OK(result.get());
}

TEST_F(SubscriberIntegrationTest, ExactlyOnce) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber =
      Subscriber(MakeSubscriberConnection(exactly_once_subscription_));

  std::mutex mu;
  std::map<std::string, int> ids;
  for (auto const* data : {"message-0", "message-1", "message-2"}) {
    auto response =
        publisher.Publish(MessageBuilder{}.SetData(data).Build()).get();
    EXPECT_STATUS_OK(response);
    if (response) {
      std::lock_guard<std::mutex> lk(mu);
      ids.emplace(*std::move(response), 0);
    }
  }
  EXPECT_FALSE(ids.empty());

  promise<void> ids_empty;
  auto ids_empty_future = ids_empty.get_future();
  auto callback = [&](pubsub::Message const& m, ExactlyOnceAckHandler h) {
    SCOPED_TRACE("Search for message " + m.message_id());
    std::unique_lock<std::mutex> lk(mu);
    auto i = ids.find(m.message_id());
    ASSERT_FALSE(i == ids.end());
    if (i->second == 0) {
      ++i->second;
      lk.unlock();
      std::move(h).nack().then([id = m.message_id()](auto f) {
        auto status = f.get();
        ASSERT_STATUS_OK(status) << " nack() failed for id=" << id;
      });
      return;
    }
    ids.erase(i);
    auto const empty = ids.empty();
    lk.unlock();
    auto done = std::move(h).ack().then([id = m.message_id()](auto f) {
      auto status = f.get();
      ASSERT_STATUS_OK(status) << " ack() failed for id=" << id;
    });
    if (!empty) return;
    done.then([p = std::move(ids_empty)](auto) mutable { p.set_value(); });
  };

  auto result = subscriber.Subscribe(callback);
  // Wait until there are no more ids pending, then cancel the subscription and
  // get its status.
  ids_empty_future.get();
  result.cancel();
  EXPECT_STATUS_OK(result.get());
}

TEST_F(SubscriberIntegrationTest, BlockingPull) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber =
      Subscriber(MakeSubscriberConnection(exactly_once_subscription_));

  std::set<std::string> ids;
  for (auto const* data : {"message-0", "message-1", "message-2"}) {
    auto response =
        publisher.Publish(MessageBuilder{}.SetData(data).Build()).get();
    EXPECT_STATUS_OK(response);
    if (response) ids.insert(*std::move(response));
  }
  EXPECT_THAT(ids, Not(IsEmpty()));

  auto const count = 2 * ids.size();
  for (std::size_t i = 0; i != count && !ids.empty(); ++i) {
    auto response = subscriber.Pull();
    EXPECT_STATUS_OK(response);
    if (!response) continue;
    auto ack = std::move(response->handler).ack().get();
    EXPECT_STATUS_OK(ack);
    ids.erase(response->message.message_id());
  }
  EXPECT_THAT(ids, IsEmpty());
}

TEST_F(SubscriberIntegrationTest, TracingEnabledPublishStreamingPullAck) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber = Subscriber(MakeSubscriberConnection(
      subscription_,
      google::cloud::Options{}.set<OpenTelemetryTracingOption>(true)));
  ASSERT_NO_FATAL_FAILURE(TestRoundtrip(publisher, subscriber));
}

TEST_F(SubscriberIntegrationTest, TracingEnabledBlockingPull) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber = Subscriber(MakeSubscriberConnection(
      exactly_once_subscription_,
      google::cloud::Options{}.set<OpenTelemetryTracingOption>(true)));

  std::set<std::string> ids;
  for (auto const* data : {"message-0", "message-1", "message-2"}) {
    auto response =
        publisher.Publish(MessageBuilder{}.SetData(data).Build()).get();
    EXPECT_STATUS_OK(response);
    if (response) ids.insert(*std::move(response));
  }
  EXPECT_THAT(ids, Not(IsEmpty()));

  auto const count = 2 * ids.size();
  for (std::size_t i = 0; i != count && !ids.empty(); ++i) {
    auto response = subscriber.Pull();
    EXPECT_STATUS_OK(response);
    if (!response) continue;
    auto ack = std::move(response->handler).ack().get();
    EXPECT_STATUS_OK(ack);
    ids.erase(response->message.message_id());
  }
  EXPECT_THAT(ids, IsEmpty());
}

TEST_F(SubscriberIntegrationTest, TracingDisabledBlockingPull) {
  auto publisher = Publisher(MakePublisherConnection(topic_));
  auto subscriber = Subscriber(MakeSubscriberConnection(
      exactly_once_subscription_,
      google::cloud::Options{}.set<OpenTelemetryTracingOption>(false)));

  std::set<std::string> ids;
  for (auto const* data : {"message-0", "message-1", "message-2"}) {
    auto response =
        publisher.Publish(MessageBuilder{}.SetData(data).Build()).get();
    EXPECT_STATUS_OK(response);
    if (response) ids.insert(*std::move(response));
  }
  EXPECT_THAT(ids, Not(IsEmpty()));

  auto const count = 2 * ids.size();
  for (std::size_t i = 0; i != count && !ids.empty(); ++i) {
    auto response = subscriber.Pull();
    EXPECT_STATUS_OK(response);
    if (!response) continue;
    auto ack = std::move(response->handler).ack().get();
    EXPECT_STATUS_OK(ack);
    ids.erase(response->message.message_id());
  }
  EXPECT_THAT(ids, IsEmpty());
}

/// @test Verify the backwards compatibility `v1` namespace still exists.
TEST_F(SubscriberIntegrationTest, BackwardsCompatibility) {
  auto connection = ::google::cloud::pubsub::v1::MakeSubscriberConnection(
      subscription_, Options{});
  EXPECT_THAT(connection, NotNull());
  ASSERT_NO_FATAL_FAILURE(Subscriber(std::move(connection)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
