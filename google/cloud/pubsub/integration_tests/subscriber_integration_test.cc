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

#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/pubsub/topic_admin_client.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

class SubscriberIntegrationTest : public ::testing::Test {
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

    auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
    auto subscription_admin =
        SubscriptionAdminClient(MakeSubscriptionAdminConnection());

    auto topic_metadata = topic_admin.CreateTopic(TopicBuilder(topic_));
    ASSERT_THAT(topic_metadata, AnyOf(StatusIs(StatusCode::kOk),
                                      StatusIs(StatusCode::kAlreadyExists)));
    auto subscription_metadata = subscription_admin.CreateSubscription(
        topic_, subscription_,
        SubscriptionBuilder{}.set_ack_deadline(std::chrono::seconds(10)));
    ASSERT_THAT(
        subscription_metadata,
        AnyOf(StatusIs(StatusCode::kOk), StatusIs(StatusCode::kAlreadyExists)));
    auto ordered_subscription_metadata = subscription_admin.CreateSubscription(
        topic_, ordered_subscription_,
        SubscriptionBuilder{}
            .set_ack_deadline(std::chrono::seconds(30))
            .enable_message_ordering(true));
    ASSERT_THAT(
        ordered_subscription_metadata,
        AnyOf(StatusIs(StatusCode::kOk), StatusIs(StatusCode::kAlreadyExists)));
  }

  void TearDown() override {
    auto topic_admin = TopicAdminClient(MakeTopicAdminConnection());
    auto subscription_admin =
        SubscriptionAdminClient(MakeSubscriptionAdminConnection());

    auto delete_ordered_subscription =
        subscription_admin.DeleteSubscription(ordered_subscription_);
    EXPECT_THAT(
        delete_ordered_subscription,
        AnyOf(StatusIs(StatusCode::kOk), StatusIs(StatusCode::kNotFound)));
    auto delete_subscription =
        subscription_admin.DeleteSubscription(subscription_);
    EXPECT_THAT(delete_subscription, AnyOf(StatusIs(StatusCode::kOk),
                                           StatusIs(StatusCode::kNotFound)));
    auto delete_topic = topic_admin.DeleteTopic(topic_);
    EXPECT_THAT(delete_topic, AnyOf(StatusIs(StatusCode::kOk),
                                    StatusIs(StatusCode::kNotFound)));
  }

  google::cloud::internal::DefaultPRNG generator_;
  Topic topic_ = Topic("unused", "unused");
  Subscription subscription_ = Subscription("unused", "unused");
  Subscription ordered_subscription_ = Subscription("unused", "unused");
};

TEST_F(SubscriberIntegrationTest, RawStub) {
  auto publisher = Publisher(MakePublisherConnection(topic_, {}));

  internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto stub = pubsub_internal::CreateDefaultSubscriberStub({}, 0);
  google::pubsub::v1::StreamingPullRequest request;
  request.set_client_id("test-client-0001");
  request.set_subscription(subscription_.FullName());
  request.set_max_outstanding_messages(1000);
  request.set_stream_ack_deadline_seconds(600);

  auto stream = [&stub, &request](CompletionQueue cq) {
    return stub->AsyncStreamingPull(
        cq, absl::make_unique<grpc::ClientContext>(), request);
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

  EXPECT_THAT(stream->Finish().get(), AnyOf(StatusIs(StatusCode::kOk),
                                            StatusIs(StatusCode::kCancelled)));
}

TEST_F(SubscriberIntegrationTest, StreamingSubscriptionBatchSource) {
  auto publisher = Publisher(MakePublisherConnection(
      topic_, {},
      pubsub::ConnectionOptions{}.set_background_thread_pool_size(2)));

  internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto stub = pubsub_internal::CreateDefaultSubscriberStub({}, 0);

  auto shutdown = std::make_shared<pubsub_internal::SessionShutdownManager>();
  auto source =
      std::make_shared<pubsub_internal::StreamingSubscriptionBatchSource>(
          background.cq(), shutdown, stub, subscription_.FullName(),
          "test-client-0001", pubsub::SubscriberOptions{},
          pubsub_testing::TestRetryPolicy(),
          pubsub_testing::TestBackoffPolicy());

  std::mutex callback_mu;
  std::condition_variable callback_cv;
  std::vector<std::string> received_ids;
  int ack_count = 0;
  int callback_count = 0;
  auto wait_ack_count = [&](int count) {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return ack_count >= count; });
  };
  auto callback =
      [&](StatusOr<google::pubsub::v1::StreamingPullResponse> const& response) {
        ASSERT_STATUS_OK(response);
        std::cout << "callback(" << response->received_messages_size() << ")"
                  << std::endl;
        {
          std::lock_guard<std::mutex> lk(callback_mu);
          for (auto const& m : response->received_messages()) {
            received_ids.push_back(m.message().message_id());
          }
          ++callback_count;
          for (auto const& m : response->received_messages()) {
            source->AckMessage(m.ack_id());
          }
          ack_count += response->received_messages_size();
        }
        callback_cv.notify_one();
      };

  auto done = shutdown->Start({});
  source->Start(std::move(callback));

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

  wait_ack_count(kPublishCount);
  {
    std::lock_guard<std::mutex> lk(callback_mu);
    for (auto const& id : received_ids) {
      expected_ids.erase(id);
    }
    EXPECT_TRUE(expected_ids.empty());
  }

  shutdown->MarkAsShutdown("test", {});
  source->Shutdown();
  EXPECT_STATUS_OK(done.get());
}

TEST_F(SubscriberIntegrationTest, PublishPullAck) {
  auto publisher = Publisher(MakePublisherConnection(topic_, {}));
  auto subscriber = Subscriber(MakeSubscriberConnection(subscription_));

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

TEST_F(SubscriberIntegrationTest, FireAndForget) {
  std::mutex mu;
  std::condition_variable cv;
  std::set<std::string> received;
  Status subscription_result;
  std::set<std::string> published;
  std::vector<Status> publish_errors;
  auto constexpr kMinimumMessages = 10;

  auto publisher = Publisher(MakePublisherConnection(topic_, {}));
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
  auto publisher = Publisher(MakePublisherConnection(topic_, {}));
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
      topic_, pubsub::PublisherOptions{}.enable_message_ordering()));
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

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
