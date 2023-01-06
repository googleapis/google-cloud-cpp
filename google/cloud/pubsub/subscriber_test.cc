// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/mocks/mock_ack_handler.h"
#include "google/cloud/pubsub/mocks/mock_subscriber_connection.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Return;

struct TestOptionA {
  using Type = std::string;
};

struct TestOptionB {
  using Type = std::string;
};

struct TestOptionC {
  using Type = std::string;
};

/// @test Verify Subscriber::Subscribe() works, including mocks.
TEST(SubscriberTest, SubscribeSimple) {
  Subscription const subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_mocks::MockSubscriberConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Subscribe)
      .WillOnce([&](SubscriberConnection::SubscribeParams const& p) {
        {
          auto ack = absl::make_unique<pubsub_mocks::MockAckHandler>();
          EXPECT_CALL(*ack, ack()).Times(1);
          p.callback(pubsub::MessageBuilder{}.SetData("do-ack").Build(),
                     AckHandler(std::move(ack)));
        }

        {
          auto ack = absl::make_unique<pubsub_mocks::MockAckHandler>();
          EXPECT_CALL(*ack, nack()).Times(1);
          p.callback(pubsub::MessageBuilder{}.SetData("do-nack").Build(),
                     AckHandler(std::move(ack)));
        }

        return make_ready_future(Status{});
      });

  Subscriber subscriber(mock);
  auto status = subscriber
                    .Subscribe([&](Message const& m, AckHandler h) {
                      if (m.data() == "do-nack") {
                        std::move(h).nack();
                      } else {
                        std::move(h).ack();
                      }
                    })
                    .get();
  ASSERT_STATUS_OK(status);
}

/// @test Verify Subscriber::Subscribe() works, including mocks.
TEST(SubscriberTest, SubscribeWithOptions) {
  Subscription const subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_mocks::MockSubscriberConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Subscribe)
      .WillOnce([&](SubscriberConnection::SubscribeParams const&) {
        return make_ready_future(Status{});
      });

  Subscriber subscriber(mock);
  auto handler = [&](Message const&, AckHandler const&) {};
  auto status = subscriber.Subscribe(std::move(handler)).get();
  ASSERT_STATUS_OK(status);
}

TEST(SubscriberTest, OptionsNoOverrides) {
  Subscription const subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_mocks::MockSubscriberConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(Return(Options{}
                                 .set<TestOptionA>("test-a")
                                 .set<TestOptionB>("test-b")
                                 .set<TestOptionC>("test-c")));
  EXPECT_CALL(*mock, Subscribe).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "test-a");
    EXPECT_EQ(current.get<TestOptionB>(), "test-b");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, ExactlyOnceSubscribe).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "test-a");
    EXPECT_EQ(current.get<TestOptionB>(), "test-b");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return make_ready_future(Status{});
  });

  Subscriber subscriber(mock);
  ASSERT_STATUS_OK(
      subscriber.Subscribe([](Message const&, AckHandler const&) {}).get());
  ASSERT_STATUS_OK(
      subscriber.Subscribe([](Message const&, ExactlyOnceAckHandler const&) {})
          .get());
}

TEST(SubscriberTest, OptionsClientOverrides) {
  Subscription const subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_mocks::MockSubscriberConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(Return(Options{}
                                 .set<TestOptionA>("test-a")
                                 .set<TestOptionB>("test-b")
                                 .set<TestOptionC>("test-c")));
  EXPECT_CALL(*mock, Subscribe).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "override-a");
    EXPECT_EQ(current.get<TestOptionB>(), "test-b");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, ExactlyOnceSubscribe).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "override-a");
    EXPECT_EQ(current.get<TestOptionB>(), "test-b");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return make_ready_future(Status{});
  });

  Subscriber subscriber(mock, Options{}.set<TestOptionA>("override-a"));
  ASSERT_STATUS_OK(
      subscriber.Subscribe([](Message const&, AckHandler const&) {}).get());
  ASSERT_STATUS_OK(
      subscriber.Subscribe([](Message const&, ExactlyOnceAckHandler const&) {})
          .get());
}

TEST(SubscriberTest, OptionsFunctionOverrides) {
  Subscription const subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_mocks::MockSubscriberConnection>();
  EXPECT_CALL(*mock, options)
      .WillRepeatedly(Return(Options{}
                                 .set<TestOptionA>("test-a")
                                 .set<TestOptionB>("test-b")
                                 .set<TestOptionC>("test-c")));
  EXPECT_CALL(*mock, Subscribe).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "override-a1");
    EXPECT_EQ(current.get<TestOptionB>(), "override-b1");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, ExactlyOnceSubscribe).WillOnce([](auto const&) {
    auto const& current = google::cloud::internal::CurrentOptions();
    EXPECT_EQ(current.get<TestOptionA>(), "override-a2");
    EXPECT_EQ(current.get<TestOptionB>(), "override-b2");
    EXPECT_EQ(current.get<TestOptionC>(), "test-c");
    return make_ready_future(Status{});
  });

  Subscriber subscriber(mock, Options{}.set<TestOptionA>("override-a"));
  ASSERT_STATUS_OK(subscriber
                       .Subscribe([](Message const&, AckHandler const&) {},
                                  Options{}
                                      .set<TestOptionA>("override-a1")
                                      .set<TestOptionB>("override-b1"))
                       .get());
  ASSERT_STATUS_OK(
      subscriber
          .Subscribe([](Message const&, ExactlyOnceAckHandler const&) {},
                     Options{}
                         .set<TestOptionA>("override-a2")
                         .set<TestOptionB>("override-b2"))
          .get());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
