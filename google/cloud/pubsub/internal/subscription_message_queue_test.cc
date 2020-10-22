// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/subscription_message_queue.h"
#include "google/cloud/pubsub/testing/mock_subscription_batch_source.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

std::vector<google::pubsub::v1::ReceivedMessage> GenerateMessages() {
  auto constexpr kTextM0 = R"pb(
    message {
      data: "m0"
      attributes: { key: "k0" value: "m0-l0" }
      attributes: { key: "k1" value: "m0-l1" }
      message_id: "id-m0"
    }
    ack_id: "ack-m0"
  )pb";
  auto constexpr kTextM1 = R"pb(
    message {
      data: "m1"
      attributes: { key: "k0" value: "m1-l0" }
      attributes: { key: "k1" value: "m1-l1" }
      message_id: "id-m1"
    }
    ack_id: "ack-m1"
  )pb";
  auto constexpr kTextM2 = R"pb(
    message {
      data: "m2"
      attributes: { key: "k0" value: "m2-l0" }
      attributes: { key: "k1" value: "m2-l1" }
      message_id: "id-m2"
    }
    ack_id: "ack-m2"
  )pb";
  google::pubsub::v1::ReceivedMessage m0;
  if (!google::protobuf::TextFormat::ParseFromString(kTextM0, &m0)) return {};
  google::pubsub::v1::ReceivedMessage m1;
  if (!google::protobuf::TextFormat::ParseFromString(kTextM1, &m1)) return {};
  google::pubsub::v1::ReceivedMessage m2;
  if (!google::protobuf::TextFormat::ParseFromString(kTextM2, &m2)) return {};

  return {m0, m1, m2};
}

google::pubsub::v1::StreamingPullResponse AsPullResponse(
    std::vector<google::pubsub::v1::ReceivedMessage> messages) {
  google::pubsub::v1::StreamingPullResponse response;
  for (auto& m : messages) {
    *response.add_received_messages() = std::move(m);
  }
  return response;
}

TEST(SubscriptionMessageQueueTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown).Times(1);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  EXPECT_CALL(*mock, AckMessage("ack-m0")).WillOnce([](std::string const&) {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, NackMessage("ack-m1")).WillOnce([](std::string const&) {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*mock, BulkNack).Times(1);

  auto const messages = GenerateMessages();
  ASSERT_FALSE(messages.empty());
  auto const response = AsPullResponse(messages);

  std::vector<google::pubsub::v1::ReceivedMessage> received;
  auto handler = [&received](google::pubsub::v1::ReceivedMessage m) {
    received.push_back(std::move(m));
  };

  auto shutdown = std::make_shared<SessionShutdownManager>();
  shutdown->Start({});
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  uut->Start(handler);

  uut->Read(1);
  EXPECT_THAT(received, ElementsAre());

  batch_callback(response);
  uut->Read(1);

  EXPECT_THAT(received, ElementsAre(IsProtoEqual(messages[0]),
                                    IsProtoEqual(messages[1])));
  uut->AckMessage("ack-m0", 0);
  uut->NackMessage("ack-m1", 1);

  received.clear();
  uut->Shutdown();
}

/// @test Verify that messages received after a shutdown are nacked.
TEST(SubscriptionMessageQueueTest, NackOnSessionShutdown) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  EXPECT_CALL(*mock, BulkNack)
      .WillOnce([&](std::vector<std::string> const& ack_ids) {
        EXPECT_THAT(ack_ids, ElementsAre("ack-m0", "ack-m1", "ack-m2"));
        return make_ready_future(Status{});
      });

  ::testing::MockFunction<void(google::pubsub::v1::ReceivedMessage const&)>
      mock_handler;

  auto const response = AsPullResponse(GenerateMessages());
  EXPECT_FALSE(response.received_messages().empty());

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  uut->Start(mock_handler.AsStdFunction());
  uut->Read(1);
  shutdown->MarkAsShutdown("test", {});
  batch_callback(response);
  uut->Shutdown();
}

/// @test Verify that receiving an error triggers the right shutdown.
TEST(SubscriptionMessageQueueTest, HandleError) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  ::testing::MockFunction<void(google::pubsub::v1::ReceivedMessage const&)>
      mock_handler;

  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  auto done = shutdown->Start({});
  uut->Start(mock_handler.AsStdFunction());
  uut->Read(1);
  auto const expected = Status{StatusCode::kPermissionDenied, "uh-oh"};
  batch_callback(expected);

  EXPECT_EQ(done.get(), expected);
}

std::vector<google::pubsub::v1::ReceivedMessage> GenerateOrderKeyMessages(
    std::string const& key, int start, int count) {
  std::vector<google::pubsub::v1::ReceivedMessage> result;
  for (int i = 0; i != count; ++i) {
    auto const id = key + "-" + absl::StrFormat("%04d", start + i);
    google::pubsub::v1::ReceivedMessage m;
    m.mutable_message()->set_message_id("id-" + id);
    m.mutable_message()->set_data("m-" + id);
    m.mutable_message()->set_ordering_key(key);
    m.set_ack_id("ack-" + id);
    result.push_back(std::move(m));
  }
  return result;
}

/// @test Verify messages with ordering keys are delivered in order
TEST(SubscriptionMessageQueueTest, RespectOrderingKeys) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown).Times(1);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  EXPECT_CALL(*mock, AckMessage).Times(AtLeast(1));
  EXPECT_CALL(*mock, NackMessage).Times(AtLeast(1));
  EXPECT_CALL(*mock, BulkNack).Times(0);

  absl::flat_hash_map<std::string, std::vector<std::string>> received;
  auto handler = [&received](google::pubsub::v1::ReceivedMessage const& m) {
    auto key = m.message().ordering_key();
    received[key].push_back(m.message().message_id());
  };

  auto shutdown = std::make_shared<SessionShutdownManager>();
  shutdown->Start({});
  // Create the queue and allow it to push 5 messages
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);
  uut->Start(handler);
  uut->Read(5);

  // Generate some messages, expecting only one for each key.
  batch_callback(AsPullResponse(GenerateOrderKeyMessages("k0", 0, 3)));
  batch_callback(AsPullResponse(GenerateOrderKeyMessages("k1", 0, 3)));
  batch_callback(AsPullResponse(GenerateOrderKeyMessages({}, 0, 3)));

  EXPECT_THAT(received["k0"], ElementsAre("id-k0-0000"));
  EXPECT_THAT(received["k1"], ElementsAre("id-k1-0000"));
  EXPECT_THAT(received[{}], ElementsAre("id--0000", "id--0001", "id--0002"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack--0000", 0);
  uut->AckMessage("ack--0001", 0);
  uut->AckMessage("ack--0002", 0);
  EXPECT_THAT(received["k0"], IsEmpty());
  EXPECT_THAT(received["k1"], IsEmpty());
  EXPECT_THAT(received[{}], IsEmpty());

  received.clear();  // keep expectations shorter
  uut->NackMessage("ack-k0-0000", 0);
  uut->Read(4);
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-0001"));
  EXPECT_THAT(received["k1"], IsEmpty());
  EXPECT_THAT(received[{}], IsEmpty());

  uut->NackMessage("ack-k1-0000", 0);
  uut->Read(1);
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-0001"));
  EXPECT_THAT(received["k1"], ElementsAre("id-k1-0001"));
  EXPECT_THAT(received[{}], IsEmpty());

  received.clear();  // keep expectations shorter
  batch_callback(AsPullResponse(GenerateOrderKeyMessages({}, 3, 2)));
  uut->AckMessage("ack-k0-0001", 0);
  uut->AckMessage("ack-k1-0001", 0);
  uut->Read(2);
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-0002"));
  EXPECT_THAT(received["k1"], ElementsAre("id-k1-0002"));
  EXPECT_THAT(received[{}], ElementsAre("id--0003", "id--0004"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack-k0-0002", 0);
  uut->AckMessage("ack-k1-0002", 0);
  uut->AckMessage("ack--0003", 0);
  uut->AckMessage("ack--0004", 0);
  uut->Read(4);
  EXPECT_THAT(received["k0"], IsEmpty());
  EXPECT_THAT(received["k1"], IsEmpty());
  EXPECT_THAT(received[{}], IsEmpty());

  batch_callback(AsPullResponse(GenerateOrderKeyMessages({}, 5, 5)));
  EXPECT_THAT(received[{}], ElementsAre("id--0005", "id--0006", "id--0007",
                                        "id--0008", "id--0009"));

  uut->Shutdown();
}

/// @test Verify messages with ordering keys are delivered in order
TEST(SubscriptionMessageQueueTest, RespectOrderingKeysTorture) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown).Times(1);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  EXPECT_CALL(*mock, AckMessage).Times(::testing::AtLeast(1));
  EXPECT_CALL(*mock, NackMessage).Times(0);
  EXPECT_CALL(*mock, BulkNack).Times(AtMost(1));

  auto shutdown = std::make_shared<SessionShutdownManager>();
  shutdown->Start({});
  // Create the queue and allow it to push many messages
  auto uut = SubscriptionMessageQueue::Create(shutdown, mock);

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(8);
  std::mutex mu;
  std::condition_variable cv;
  auto constexpr kExpectedCount = 400;
  int received_count = 0;
  absl::flat_hash_map<std::string, std::vector<std::string>> received;
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto handler = [&](google::pubsub::v1::ReceivedMessage const& m) {
    background.cq().RunAsync([&, m]() {
      auto delay = [&] {
        std::lock_guard<std::mutex> lk(mu);
        return std::uniform_int_distribution<int>(0, 500)(generator);
      }();
      std::this_thread::sleep_for(std::chrono::microseconds(delay));
      auto key = m.message().ordering_key();
      bool notify = false;
      {
        std::lock_guard<std::mutex> lk(mu);
        received[key].push_back(m.message().message_id());
        ++received_count;
        notify = received_count >= kExpectedCount;
      }
      uut->AckMessage(m.ack_id(), 0);
      uut->Read(1);
      if (notify) cv.notify_one();
    });
  };

  auto constexpr kMaxCallbacks = 150;
  uut->Start(handler);
  uut->Read(kMaxCallbacks);

  auto worker = [batch_callback](std::string const& key, int count, int step) {
    auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());
    for (int i = 0; i < count; i += step) {
      batch_callback(AsPullResponse(GenerateOrderKeyMessages(key, i, step)));
      auto delay = std::uniform_int_distribution<int>(0, 500)(gen);
      std::this_thread::sleep_for(std::chrono::microseconds(delay));
    }
  };
  std::vector<std::thread> tasks;
  tasks.emplace_back(worker, std::string("k0"), 100, 3);
  tasks.emplace_back(worker, std::string("k1"), 100, 5);
  tasks.emplace_back(worker, std::string("k2"), 100, 7);
  tasks.emplace_back(worker, std::string{}, 100, 11);

  {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return received_count >= kExpectedCount; });
  }

  for (auto& t : tasks) t.join();
  background.Shutdown();  // This waits for the threads

  for (auto& kv : received) {
    SCOPED_TRACE("Testing messages for key=<" + kv.first + ">");
    EXPECT_FALSE(kv.second.empty());
    if (kv.first.empty()) continue;
    std::vector<std::string> ids = kv.second;
    std::sort(ids.begin(), ids.end());
    EXPECT_THAT(kv.second, ElementsAreArray(ids.begin(), ids.end()));
  }

  uut->Shutdown();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
