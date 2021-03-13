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

#include "google/cloud/pubsub/internal/batching_publisher_connection.h"
#include "google/cloud/pubsub/testing/mock_batch_sink.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <numeric>
#include <random>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

google::pubsub::v1::PublishResponse MakeResponse(
    google::pubsub::v1::PublishRequest const& request) {
  google::pubsub::v1::PublishResponse response;
  for (auto const& m : request.messages()) {
    response.add_message_ids("id-" + m.message_id());
  }
  return response;
}

TEST(BatchingPublisherConnectionTest, DefaultMakesProgress) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  AsyncSequencer<void> async;
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        return async.PushBack().then([topic, request](future<void>) {
          EXPECT_EQ(topic.FullName(), request.topic());
          std::vector<std::string> data;
          std::transform(request.messages().begin(), request.messages().end(),
                         std::back_inserter(data),
                         [](google::pubsub::v1::PubsubMessage const& m) {
                           return std::string(m.data());
                         });
          EXPECT_THAT(data, ElementsAre("test-data-0", "test-data-1"));
          google::pubsub::v1::PublishResponse response;
          response.add_message_ids("test-message-id-0");
          response.add_message_ids("test-message-id-1");
          return make_status_or(response);
        });
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(4)
          .set_maximum_hold_time(std::chrono::milliseconds(50)),
      ordering_key, mock, background.cq());

  // We expect the responses to be satisfied in the context of the completion
  // queue threads. This is an important property, the processing of any
  // responses should be scheduled with any other work.
  auto const main_thread = std::this_thread::get_id();
  std::vector<future<void>> published;
  published.push_back(
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-0").Build()})
          .then([&](future<StatusOr<std::string>> f) {
            auto r = f.get();
            ASSERT_STATUS_OK(r);
            EXPECT_EQ("test-message-id-0", *r);
            EXPECT_NE(main_thread, std::this_thread::get_id());
          }));
  published.push_back(
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-1").Build()})
          .then([&](future<StatusOr<std::string>> f) {
            auto r = f.get();
            ASSERT_STATUS_OK(r);
            EXPECT_EQ("test-message-id-1", *r);
            EXPECT_NE(main_thread, std::this_thread::get_id());
          }));
  publisher->Flush({});
  // Use the CQ threads to satisfy the AsyncPull future, like we do in the
  // normal code.
  background.cq().RunAsync([&async] { async.PopFront().set_value(); });
  for (auto& p : published) p.get();
}

TEST(BatchingPublisherConnectionTest, BatchByMessageCount) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        EXPECT_EQ("test-data-1", request.messages(1).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  // Make this so large that the test times out before the message hold expires.
  // We could control the CompletionQueue activation, but that is more tedious.
  auto constexpr kMaxHoldTime = std::chrono::hours(24);
  // Likewise, this is too large to trigger a flush in this test.
  auto constexpr kMaxBytes = 10 * 1024 * 1024;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(2)
          .set_maximum_hold_time(kMaxHoldTime)
          .set_maximum_batch_bytes(kMaxBytes),
      ordering_key, mock, background.cq());
  auto r0 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-0").Build()})
          .then([](future<StatusOr<std::string>> f) {
            auto r = f.get();
            ASSERT_STATUS_OK(r);
            EXPECT_EQ("test-message-id-0", *r);
          });
  auto r1 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-1").Build()})
          .then([](future<StatusOr<std::string>> f) {
            auto r = f.get();
            ASSERT_STATUS_OK(r);
            EXPECT_EQ("test-message-id-1", *r);
          });

  r0.get();
  r1.get();
  background.cq().CancelAll();
}

TEST(BatchingPublisherConnectionTest, BatchByMessageSize) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        EXPECT_EQ("test-data-1", request.messages(1).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        return make_ready_future(make_status_or(response));
      });

  // Compute a message size that is exactly met by the two messages we test
  // with.
  auto m0 = pubsub::MessageBuilder{}.SetData("test-data-0").Build();
  auto m1 = pubsub::MessageBuilder{}.SetData("test-data-1").Build();
  auto const max_bytes = MessageSize(m0) + MessageSize(m1);
  // Make this so large that the test times out before the message hold expires.
  // We could control the CompletionQueue activation, but that is more tedious.
  auto constexpr kMaxHoldTime = std::chrono::hours(24);
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(4)
          .set_maximum_batch_bytes(max_bytes)
          .set_maximum_hold_time(kMaxHoldTime),
      ordering_key, mock, background.cq());
  auto r0 = publisher->Publish({m0}).then([](future<StatusOr<std::string>> f) {
    auto r = f.get();
    ASSERT_STATUS_OK(r);
    EXPECT_EQ("test-message-id-0", *r);
  });
  auto r1 = publisher->Publish({m1}).then([](future<StatusOr<std::string>> f) {
    auto r = f.get();
    ASSERT_STATUS_OK(r);
    EXPECT_EQ("test-message-id-1", *r);
  });

  r0.get();
  r1.get();
  background.cq().CancelAll();
}

TEST(BatchingPublisherConnectionTest, BatchByMessageSizeLargeMessageBreak) {
  pubsub::Topic const topic("test-project", "test-topic");

  auto constexpr kSinglePayload = 128;
  auto constexpr kBatchLimit = 4 * kSinglePayload;
  auto const single_payload = std::string(kSinglePayload, 'A');
  auto const double_payload = std::string(2 * kSinglePayload, 'B');

  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(3, request.messages_size());
        EXPECT_EQ(single_payload, request.messages(0).data());
        EXPECT_EQ(single_payload, request.messages(1).data());
        EXPECT_EQ(single_payload, request.messages(2).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        response.add_message_ids("test-message-id-2");
        return make_ready_future(make_status_or(response));
      })
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(double_payload, request.messages(0).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-3");
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(100)
          .set_maximum_batch_bytes(kBatchLimit),
      ordering_key, mock, background.cq());
  std::vector<future<Status>> results;
  for (int i = 0; i != 3; ++i) {
    results.push_back(
        publisher
            ->Publish(
                {pubsub::MessageBuilder{}.SetData(single_payload).Build()})
            .then([](future<StatusOr<std::string>> f) {
              return f.get().status();
            }));
  }
  // This will exceed the maximum size, it should flush the previously held
  // messages.
  results.push_back(
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData(double_payload).Build()})
          .then([](future<StatusOr<std::string>> f) {
            return f.get().status();
          }));
  publisher->Flush({});
  for (auto& r : results) EXPECT_STATUS_OK(r.get());
}

TEST(BatchingPublisherConnectionTest, BatchByMessageSizeOversizedSingleton) {
  pubsub::Topic const topic("test-project", "test-topic");

  auto constexpr kSinglePayload = 128;
  auto constexpr kBatchLimit = 4 * kSinglePayload;
  auto const single_payload = std::string(kSinglePayload, 'A');
  auto const oversized_payload = std::string(5 * kSinglePayload, 'B');

  std::atomic<int> ack_id_generator{0};
  auto generate_acks =
      [&ack_id_generator](google::pubsub::v1::PublishRequest const& r) {
        google::pubsub::v1::PublishResponse response;
        for (int i = 0; i != r.messages_size(); ++i) {
          response.add_message_ids("ack-" + std::to_string(++ack_id_generator));
        }
        return make_ready_future(make_status_or(response));
      };

  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(3, request.messages_size());
        EXPECT_EQ(single_payload, request.messages(0).data());
        EXPECT_EQ(single_payload, request.messages(1).data());
        EXPECT_EQ(single_payload, request.messages(2).data());
        return generate_acks(request);
      })
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(oversized_payload, request.messages(0).data());
        return generate_acks(request);
      })
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(3, request.messages_size());
        EXPECT_EQ(single_payload, request.messages(0).data());
        EXPECT_EQ(single_payload, request.messages(1).data());
        EXPECT_EQ(single_payload, request.messages(2).data());
        return generate_acks(request);
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(100)
          .set_maximum_batch_bytes(kBatchLimit),
      ordering_key, mock, background.cq());
  std::vector<future<Status>> results;
  auto publish_single = [&] {
    results.push_back(
        publisher
            ->Publish(
                {pubsub::MessageBuilder{}.SetData(single_payload).Build()})
            .then([](future<StatusOr<std::string>> f) {
              return f.get().status();
            }));
  };
  for (int i = 0; i != 3; ++i) publish_single();
  // This will exceed the maximum size, it should flush the previously held
  // messages *and* it should be immediately sent because it is too large by
  // itself.
  results.push_back(
      publisher
          ->Publish(
              {pubsub::MessageBuilder{}.SetData(oversized_payload).Build()})
          .then([](future<StatusOr<std::string>> f) {
            return f.get().status();
          }));
  for (int i = 0; i != 3; ++i) publish_single();
  publisher->Flush({});
  for (auto& r : results) EXPECT_STATUS_OK(r.get());
}

TEST(BatchingPublisherConnectionTest, BatchTorture) {
  pubsub::Topic const topic("test-project", "test-topic");

  auto constexpr kMaxMessages = 20;
  auto constexpr kMaxSinglePayload = 2048;
  auto constexpr kMaxPayload = kMaxMessages * kMaxSinglePayload / 2;

  std::atomic<int> ack_id_generator{0};
  auto generate_acks =
      [&ack_id_generator](google::pubsub::v1::PublishRequest const& r) {
        google::pubsub::v1::PublishResponse response;
        for (int i = 0; i != r.messages_size(); ++i) {
          response.add_message_ids("ack-" + std::to_string(++ack_id_generator));
        }
        return make_ready_future(make_status_or(response));
      };

  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_LE(request.messages_size(), kMaxMessages);
        std::size_t size = 0;
        for (auto const& m : request.messages()) size += MessageProtoSize(m);
        EXPECT_LE(size, kMaxPayload);
        return generate_acks(request);
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(kMaxMessages)
          .set_maximum_batch_bytes(kMaxPayload),
      ordering_key, mock, background.cq());

  auto worker = [&](int iterations) {
    auto gen = google::cloud::internal::DefaultPRNG(std::random_device{}());

    auto publish_single = [&] {
      auto const size =
          std::uniform_int_distribution<std::size_t>(0, kMaxSinglePayload)(gen);
      return publisher
          ->Publish({pubsub::MessageBuilder{}
                         .SetData(std::string(size, 'Y'))
                         .Build()})
          .then(
              [](future<StatusOr<std::string>> f) { return f.get().status(); });
    };
    std::vector<future<Status>> results;
    for (int i = 0; i != iterations; ++i) results.push_back(publish_single());
    for (auto& r : results) EXPECT_STATUS_OK(r.get());
  };
  std::vector<std::thread> workers(4);
  std::generate(workers.begin(), workers.end(), [&] {
    return std::thread{worker, 1000};
  });
  publisher->Flush({});
  for (auto& w : workers) w.join();
}

TEST(BatchingPublisherConnectionTest, BatchByMaximumHoldTime) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        EXPECT_EQ("test-data-1", request.messages(1).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        return make_ready_future(make_status_or(response));
      });

  // Start with an inactive message queue, to avoid flakes due to scheduling
  // problems.
  CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_hold_time(std::chrono::milliseconds(5))
          .set_maximum_batch_message_count(4),
      /*ordering_key=*/{}, mock, cq);
  auto r0 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-0").Build()})
          .then([](future<StatusOr<std::string>> f) {
            auto r = f.get();
            ASSERT_STATUS_OK(r);
            EXPECT_EQ("test-message-id-0", *r);
          });
  auto r1 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-1").Build()})
          .then([](future<StatusOr<std::string>> f) {
            auto r = f.get();
            ASSERT_STATUS_OK(r);
            EXPECT_EQ("test-message-id-1", *r);
          });

  // Now that the two messages are queued, we can activate the completion queue.
  // It should flush the messages in about 5ms.
  std::thread t{[](CompletionQueue cq) { cq.Run(); }, cq};

  r0.get();
  r1.get();

  cq.Shutdown();
  t.join();
}

TEST(BatchingPublisherConnectionTest, BatchByFlush) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        EXPECT_EQ("test-data-1", request.messages(1).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        return make_ready_future(make_status_or(response));
      })
      .WillRepeatedly([&](google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("ack-for-" + std::string(m.data()));
        }
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_hold_time(std::chrono::milliseconds(5))
          .set_maximum_batch_message_count(4),
      ordering_key, mock, background.cq());

  std::vector<future<void>> results;
  for (auto i : {0, 1}) {
    results.push_back(
        publisher
            ->Publish({pubsub::MessageBuilder{}
                           .SetData("test-data-" + std::to_string(i))
                           .Build()})
            .then([i](future<StatusOr<std::string>> f) {
              auto r = f.get();
              ASSERT_STATUS_OK(r);
              EXPECT_EQ("test-message-id-" + std::to_string(i), *r);
            }));
  }

  // Trigger the first `.WillOnce()` expectation.  CQ is not running yet, so the
  // flush cannot be explained by a timer, and the message count is too low.
  publisher->Flush({});

  for (auto i : {2, 3, 4}) {
    auto data = std::string{"test-data-"} + std::to_string(i);
    results.push_back(
        publisher->Publish({pubsub::MessageBuilder{}.SetData(data).Build()})
            .then([data](future<StatusOr<std::string>> f) {
              auto r = f.get();
              ASSERT_STATUS_OK(r);
              EXPECT_EQ("ack-for-" + data, *r);
            }));
  }

  for (auto& r : results) r.get();
}

TEST(BatchingPublisherConnectionTest, HandleError) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  auto const error_status = Status(StatusCode::kPermissionDenied, "uh-oh");
  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([&](google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(
            StatusOr<google::pubsub::v1::PublishResponse>(error_status));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads bg;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::PublisherOptions{}.set_maximum_batch_message_count(2),
      ordering_key, mock, bg.cq());
  auto r0 = publisher->Publish(
      {pubsub::MessageBuilder{}.SetData("test-data-0").Build()});
  auto r1 = publisher->Publish(
      {pubsub::MessageBuilder{}.SetData("test-data-1").Build()});

  EXPECT_THAT(r0.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  EXPECT_THAT(r1.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(BatchingPublisherConnectionTest, HandleInvalidResponse) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([&](google::pubsub::v1::PublishRequest const&) {
        google::pubsub::v1::PublishResponse response;
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::PublisherOptions{}.set_maximum_batch_message_count(2),
      "test-ordering-key", mock, background.cq());
  auto r0 = publisher->Publish(
      {pubsub::MessageBuilder{}.SetData("test-data-0").Build()});
  auto r1 = publisher->Publish(
      {pubsub::MessageBuilder{}.SetData("test-data-1").Build()});

  EXPECT_THAT(r0.get(), StatusIs(StatusCode::kUnknown,
                                 HasSubstr("mismatched message id count")));
  EXPECT_THAT(r1.get(), StatusIs(StatusCode::kUnknown,
                                 HasSubstr("mismatched message id count")));
}

TEST(BatchingPublisherConnectionTest, HandleErrorWithOrderingPartialBatch) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");

  auto const error_status = Status(StatusCode::kPermissionDenied, "uh-oh");

  AsyncSequencer<void> async;
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::pubsub::v1::PublishRequest const&) {
        return async.PushBack().then([error_status](future<void>) {
          return StatusOr<google::pubsub::v1::PublishResponse>(error_status);
        });
      });

  auto constexpr kBatchSize = 4;
  auto const ordering_key = std::string{"test-key"};
  // Create an inactive queue to avoid race conditions.
  google::cloud::CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}.set_maximum_batch_message_count(kBatchSize),
      ordering_key, mock, cq);
  std::vector<future<StatusOr<std::string>>> results;
  // Create a full batch (by message count) and a partial batch.
  for (int i = 0; i != kBatchSize + kBatchSize / 2; ++i) {
    results.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("data-" + std::to_string(i))
                                .Build()}));
  }

  // Satisfy the first response.
  async.PopFront().set_value();

  // The callbacks for the partial batch run asynchronously, we need to activate
  // the CompletionQueue.
  std::thread t{[](CompletionQueue cq) { cq.Run(); }, cq};

  // All results should be satisfied with an error.
  for (auto& f : results) {
    EXPECT_THAT(f.get(),
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  }
  cq.Shutdown();
  t.join();
}

TEST(BatchingPublisherConnectionTest, HandleErrorWithOrderingResume) {
  auto mock = std::make_shared<pubsub_testing::MockBatchSink>();
  pubsub::Topic const topic("test-project", "test-topic");
  auto const ordering_key = std::string{"test-key"};

  auto const error_status = Status(StatusCode::kPermissionDenied, "uh-oh");

  AsyncSequencer<void> async;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const&) {
          return async.PushBack().then([error_status](future<void>) {
            return StatusOr<google::pubsub::v1::PublishResponse>(error_status);
          });
        });

    EXPECT_CALL(*mock, ResumePublish(ordering_key));
    EXPECT_CALL(*mock, AsyncPublish)
        .WillOnce([&](google::pubsub::v1::PublishRequest const& r) {
          return async.PushBack().then(
              [r](future<void>) { return make_status_or(MakeResponse(r)); });
        });
  }

  auto constexpr kBatchSize = 4;
  auto constexpr kMaxHoldTime = std::chrono::hours(24);
  // Create an inactive queue to avoid race conditions.
  google::cloud::CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(kBatchSize)
          .set_maximum_hold_time(kMaxHoldTime),
      ordering_key, mock, cq);
  std::vector<future<StatusOr<std::string>>> results;
  // Create a full batch (by size).
  for (int i = 0; i != kBatchSize; ++i) {
    results.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("data-" + std::to_string(i))
                                .Build()}));
  }

  // Satisfy the first response.
  async.PopFront().set_value();

  // The functions to satisfy successful requests run asynchronously, we need to
  // activate the CompletionQueue.
  std::thread t{[](CompletionQueue cq) { cq.Run(); }, cq};

  // All results should be satisfied with an error.
  for (auto& f : results) {
    EXPECT_THAT(f.get(),
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  }

  // New requests should fail immediately.
  auto r = publisher->Publish(
      {pubsub::MessageBuilder{}.SetData("data-post-error").Build()});
  EXPECT_THAT(r.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));

  // After we resume the operations should succeed again.
  publisher->ResumePublish({"test-key"});
  results.clear();
  for (int i = 0; i != kBatchSize; ++i) {
    results.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("data-" + std::to_string(i))
                                .Build()}));
  }
  publisher->Flush({});

  // Satisfy the first response.
  async.PopFront().set_value();

  // All results should be satisfied with an error.
  for (auto& f : results) ASSERT_THAT(f.get(), IsOk());

  cq.CancelAll();
  cq.Shutdown();
  t.join();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
