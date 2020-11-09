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
#include "google/cloud/internal/absl_flat_hash_map_quiet.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
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

std::vector<google::pubsub::v1::ReceivedMessage> GenerateOrderKeyMessages(
    std::string const& key, int start, int count) {
  std::vector<google::pubsub::v1::ReceivedMessage> result;
  for (int i = 0; i != count; ++i) {
    auto const id = key + "-" + absl::StrFormat("%06d", start + i);
    google::pubsub::v1::ReceivedMessage m;
    m.mutable_message()->set_message_id("id-" + id);
    m.mutable_message()->set_data("m-" + id);
    m.mutable_message()->set_ordering_key(key);
    m.set_ack_id("ack-" + id);
    result.push_back(std::move(m));
  }
  return result;
}

// We need to test with different numbers of threads, ordering key sizes, etc.
struct TestParams {
  int thread_count;
  int key_count;
  int message_count;
};

// Test names may only contain alphanumeric characters, yuck.
std::ostream& operator<<(std::ostream& os, TestParams const& rhs) {
  return os << "Thread" << rhs.thread_count << "Key" << rhs.key_count
            << "Message" << rhs.message_count;
}

class SubscriptionMessageQueueOrderingTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestParams> {};

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
  uut->AckMessage("ack-m0");
  uut->NackMessage("ack-m1");

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

  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000000"));
  EXPECT_THAT(received["k1"], ElementsAre("id-k1-000000"));
  EXPECT_THAT(received[{}],
              ElementsAre("id--000000", "id--000001", "id--000002"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack--000000");
  uut->AckMessage("ack--000001");
  uut->AckMessage("ack--000002");
  EXPECT_THAT(received["k0"], IsEmpty());
  EXPECT_THAT(received["k1"], IsEmpty());
  EXPECT_THAT(received[{}], IsEmpty());

  received.clear();  // keep expectations shorter
  uut->NackMessage("ack-k0-000000");
  uut->Read(4);
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000001"));
  EXPECT_THAT(received["k1"], IsEmpty());
  EXPECT_THAT(received[{}], IsEmpty());

  uut->NackMessage("ack-k1-000000");
  uut->Read(1);
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000001"));
  EXPECT_THAT(received["k1"], ElementsAre("id-k1-000001"));
  EXPECT_THAT(received[{}], IsEmpty());

  received.clear();  // keep expectations shorter
  batch_callback(AsPullResponse(GenerateOrderKeyMessages({}, 3, 2)));
  uut->AckMessage("ack-k0-000001");
  uut->AckMessage("ack-k1-000001");
  uut->Read(2);
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000002"));
  EXPECT_THAT(received["k1"], ElementsAre("id-k1-000002"));
  EXPECT_THAT(received[{}], ElementsAre("id--000003", "id--000004"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack-k0-000002");
  uut->AckMessage("ack-k1-000002");
  uut->AckMessage("ack--000003");
  uut->AckMessage("ack--000004");
  uut->Read(4);
  EXPECT_THAT(received["k0"], IsEmpty());
  EXPECT_THAT(received["k1"], IsEmpty());
  EXPECT_THAT(received[{}], IsEmpty());

  batch_callback(AsPullResponse(GenerateOrderKeyMessages({}, 5, 5)));
  EXPECT_THAT(received[{}],
              ElementsAre("id--000005", "id--000006", "id--000007",
                          "id--000008", "id--000009"));

  uut->Shutdown();
}

/// @test Verify duplicate messages are handled correctly
TEST(SubscriptionMessageQueueTest, DuplicateMessagesNoKey) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown).Times(1);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, AckMessage("ack--000000")).Times(1);
  EXPECT_CALL(*mock, NackMessage("ack--000001")).Times(1);
  EXPECT_CALL(*mock, AckMessage("ack--000000")).Times(1);
  EXPECT_CALL(*mock, AckMessage("ack--000001")).Times(1);
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
  uut->Read(4);

  // Generate some messages, expecting only one for each key.
  auto const response = AsPullResponse(GenerateOrderKeyMessages({}, 0, 2));
  batch_callback(response);
  batch_callback(response);
  batch_callback(AsPullResponse(GenerateOrderKeyMessages({}, 2, 4)));

  EXPECT_THAT(received[{}], ElementsAre("id--000000", "id--000001",
                                        "id--000000", "id--000001"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack--000000");
  uut->NackMessage("ack--000001");
  uut->Read(2);
  EXPECT_THAT(received[{}], ElementsAre("id--000002", "id--000003"));

  uut->AckMessage("ack--000000");
  uut->AckMessage("ack--000001");
  uut->Read(2);
  EXPECT_THAT(received[{}], ElementsAre("id--000002", "id--000003",
                                        "id--000004", "id--000005"));

  uut->Shutdown();
}

/// @test Verify duplicate messages are handled correctly
TEST(SubscriptionMessageQueueTest, DuplicateMessagesWithKey) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown).Times(1);
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, AckMessage).Times(AtLeast(1));
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
  uut->Read(8);

  // Generate some messages, expecting only one for each key.
  batch_callback(AsPullResponse(GenerateOrderKeyMessages("k0", 0, 1)));
  batch_callback(AsPullResponse(GenerateOrderKeyMessages("k0", 0, 3)));
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000000"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack-k0-000000");
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000000"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack-k0-000000");
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000001"));

  received.clear();  // keep expectations shorter
  uut->AckMessage("ack-k0-000001");
  EXPECT_THAT(received["k0"], ElementsAre("id-k0-000002"));

  uut->Shutdown();
}

/// @test Work with large sets of non-keyed messages
TEST_P(SubscriptionMessageQueueOrderingTest, RespectOrderingKeysTorture) {
  auto const message_count = GetParam().message_count;
  auto const thread_count = GetParam().thread_count;
  auto const key_count = GetParam().key_count;

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

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(
      thread_count);
  std::mutex mu;
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto delay = [&] {
    std::lock_guard<std::mutex> lk(mu);
    return std::uniform_int_distribution<int>(0, 50)(generator);
  };

  std::condition_variable cv;
  std::int64_t received_count = 0;
  absl::flat_hash_map<std::string, std::vector<std::string>> received;

  auto handler = [&](google::pubsub::v1::ReceivedMessage const& m) {
    background.cq().RunAsync([&, m]() {
      std::this_thread::sleep_for(std::chrono::microseconds(delay()));
      bool notify = false;
      {
        std::lock_guard<std::mutex> lk(mu);
        received[m.message().ordering_key()].push_back(
            m.message().message_id());
        notify = (++received_count >= message_count);
      }
      if (notify) cv.notify_one();
      uut->AckMessage(m.ack_id());
      uut->Read(1);
    });
  };

  auto constexpr kMaxCallbacks = 128;
  uut->Start(handler);
  uut->Read(kMaxCallbacks);

  auto worker = [batch_callback](std::string const& key, int count) {
    auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());
    using random = std::uniform_int_distribution<int>;
    for (int i = 0; i < count;) {
      auto step = random(0, count - i)(gen);
      batch_callback(AsPullResponse(GenerateOrderKeyMessages(key, i, step)));
      i += step;
      std::this_thread::sleep_for(
          std::chrono::microseconds(random(0, 50)(gen)));
    }
  };
  std::vector<std::thread> tasks;
  auto constexpr kMinMessages = 100;
  auto const per_key_count = message_count / key_count + kMinMessages;
  tasks.emplace_back(worker, std::string{}, per_key_count);
  for (int i = 1; i != key_count; ++i) {
    tasks.emplace_back(worker, absl::StrFormat("%06d", i), per_key_count);
  }

  {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return received_count >= message_count; });
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

INSTANTIATE_TEST_SUITE_P(
    SubscriptionMessageQueueOrderingTest, SubscriptionMessageQueueOrderingTest,
    ::testing::Values(TestParams{1, 4, 1000}, TestParams{1, 1000, 1000},
                      TestParams{8, 4, 1000}, TestParams{8, 1000, 1000}),
    ::testing::PrintToStringParamName());

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
