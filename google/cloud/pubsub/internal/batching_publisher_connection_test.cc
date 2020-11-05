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
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/default_completion_queue_impl.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
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

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(BatchingPublisherConnectionTest, DefaultMakesProgress) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
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
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(4)
          .set_maximum_hold_time(std::chrono::milliseconds(50)),
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());

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
  for (auto& p : published) p.get();
}

TEST(BatchingPublisherConnectionTest, BatchByMessageCount) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
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
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::PublisherOptions{}.set_maximum_batch_message_count(2),
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
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
}

TEST(BatchingPublisherConnectionTest, BatchByMessageSize) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        EXPECT_EQ("test-data-1", request.messages(1).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        return make_ready_future(make_status_or(response));
      });

  // see https://cloud.google.com/pubsub/pricing
  auto constexpr kMessageSizeOverhead = 20;
  auto constexpr kMaxMessageBytes =
      2 * (sizeof("test-data-N") + kMessageSizeOverhead + 2);
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(4)
          .set_maximum_batch_bytes(kMaxMessageBytes),
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
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
}

TEST(BatchingPublisherConnectionTest, BatchByMessageSizeLargeMessageBreak) {
  pubsub::Topic const topic("test-project", "test-topic");

  auto constexpr kSinglePayload = 128;
  auto constexpr kBatchLimit = 4 * kSinglePayload;
  auto const single_payload = std::string(kSinglePayload, 'A');
  auto const double_payload = std::string(2 * kSinglePayload, 'B');

  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
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
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
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
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
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

  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(3, request.messages_size());
        EXPECT_EQ(single_payload, request.messages(0).data());
        EXPECT_EQ(single_payload, request.messages(1).data());
        EXPECT_EQ(single_payload, request.messages(2).data());
        return generate_acks(request);
      })
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(oversized_payload, request.messages(0).data());
        return generate_acks(request);
      })
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
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
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
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

  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const& request) {
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
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());

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
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
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
      /*ordering_key=*/{}, mock, cq, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
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
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        EXPECT_EQ("test-data-1", request.messages(1).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        response.add_message_ids("test-message-id-1");
        return make_ready_future(make_status_or(response));
      })
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const& request) {
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
      ordering_key, mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());

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
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  auto const error_status = Status(StatusCode::kPermissionDenied, "uh-oh");
  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(
            StatusOr<google::pubsub::v1::PublishResponse>(error_status));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads bg;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::PublisherOptions{}.set_maximum_batch_message_count(2),
      ordering_key, mock, bg.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto check_status = [&](future<StatusOr<std::string>> f) {
    auto r = f.get();
    EXPECT_THAT(r.status(),
                StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
  };
  auto r0 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-0").Build()})
          .then(check_status);
  auto r1 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-1").Build()})
          .then(check_status);

  r0.get();
  r1.get();
}

TEST(BatchingPublisherConnectionTest, HandleInvalidResponse) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const&) {
        google::pubsub::v1::PublishResponse response;
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::PublisherOptions{}.set_maximum_batch_message_count(2),
      "test-ordering-key", mock, background.cq(),
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  auto check_status = [&](future<StatusOr<std::string>> f) {
    auto r = f.get();
    EXPECT_EQ(StatusCode::kUnknown, r.status().code());
    EXPECT_THAT(r.status().message(), HasSubstr("mismatched message id count"));
  };
  auto r0 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-0").Build()})
          .then(check_status);
  auto r1 =
      publisher
          ->Publish({pubsub::MessageBuilder{}.SetData("test-data-1").Build()})
          .then(check_status);

  r0.get();
  r1.get();
}

TEST(BatchingPublisherConnectionTest, OrderingBatchCorked) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  std::mutex mu;
  std::condition_variable cv;
  std::deque<promise<void>> pending;
  auto add_pending = [&] {
    std::unique_lock<std::mutex> lk(mu);
    pending.emplace_back();
    cv.notify_one();
    return pending.back().get_future();
  };
  auto wait_pending = [&] {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return !pending.empty(); });
    auto p = std::move(pending.front());
    pending.pop_front();
    return p;
  };
  auto make_response = [&](int expected_count) {
    return
        [&, expected_count](google::cloud::CompletionQueue&,
                            std::unique_ptr<grpc::ClientContext>,
                            google::pubsub::v1::PublishRequest const& request) {
          EXPECT_EQ(expected_count, request.messages_size());
          google::pubsub::v1::PublishResponse response;
          for (auto const& m : request.messages()) {
            response.add_message_ids("id-" + std::string(m.data()));
          }
          return add_pending().then(
              [response](future<void>) { return make_status_or(response); });
        };
  };
  auto constexpr kBatchSize = 2;
  auto constexpr kMessageCount = 3 * kBatchSize;
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce(make_response(kBatchSize))
      .WillOnce(make_response(kMessageCount - kBatchSize));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}.set_maximum_batch_message_count(kBatchSize),
      "test-key", mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  std::vector<future<StatusOr<std::string>>> responses;
  for (int i = 0; i != kMessageCount; ++i) {
    responses.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("d-" + std::to_string(i))
                                .SetOrderingKey("test-key")
                                .Build()}));
    // Calling ResumePublish() should have no side effects.
    publisher->ResumePublish({"test-key"});
  }

  // None of the responses should be ready because the mock has not sent a
  // response
  for (auto& r : responses) {
    using ms = std::chrono::milliseconds;
    EXPECT_EQ(std::future_status::timeout, r.wait_for(ms(0)));
  }
  // Trigger the first response.
  wait_pending().set_value();
  for (int i = 0; i != kBatchSize; ++i) {
    ASSERT_STATUS_OK(responses[i].get());
  }

  // Trigger the second response.
  wait_pending().set_value();
  for (int i = kBatchSize; i != kMessageCount; ++i) {
    ASSERT_STATUS_OK(responses[i].get());
  }
}

TEST(BatchingPublisherConnectionTest, OrderingBatchErrorRejectAfterError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");
  auto const expected_status = Status{StatusCode::kPermissionDenied, "uh-oh"};

  EXPECT_CALL(*mock, AsyncPublish).Times(0);

  auto constexpr kBatchSize = 2;
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}.set_maximum_batch_message_count(kBatchSize),
      "test-key", mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());

  // Simulate a previous error
  publisher->DiscardCorked(expected_status);
  for (int i = 0; i != 3 * kBatchSize; ++i) {
    auto response = publisher
                        ->Publish({pubsub::MessageBuilder{}
                                       .SetData("d-" + std::to_string(i))
                                       .SetOrderingKey("test-key")
                                       .Build()})
                        .get();
    EXPECT_EQ(response.status(), expected_status);
  }
}

TEST(BatchingPublisherConnectionTest, OrderingBatchErrorResume) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");
  auto const expected_status = Status{StatusCode::kPermissionDenied, "uh-oh"};

  EXPECT_CALL(*mock, AsyncPublish)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::PublishRequest const& request) {
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("id-" + std::string(m.data()));
        }
        return make_ready_future(make_status_or(response));
      });

  auto constexpr kBatchSize = 2;
  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}.set_maximum_batch_message_count(kBatchSize),
      "test-key", mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());

  // Simulate a previous error
  publisher->DiscardCorked(expected_status);
  for (int i = 0; i != 3 * kBatchSize; ++i) {
    auto response = publisher
                        ->Publish({pubsub::MessageBuilder{}
                                       .SetData("d-" + std::to_string(i))
                                       .SetOrderingKey("test-key")
                                       .Build()})
                        .get();
    EXPECT_EQ(response.status(), expected_status);
  }
  publisher->ResumePublish({"test-key"});
  std::vector<future<StatusOr<std::string>>> responses;
  for (int i = 0; i != 3 * kBatchSize; ++i) {
    responses.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("d-" + std::to_string(i))
                                .SetOrderingKey("test-key")
                                .Build()}));
  }
  publisher->Flush({});
  for (auto& r : responses) {
    EXPECT_THAT(r.get().status(), StatusIs(StatusCode::kOk));
  }
}

TEST(BatchingPublisherConnectionTest, OrderingBatchDiscardOnError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  std::mutex mu;
  std::condition_variable cv;
  std::deque<promise<void>> pending;
  auto add_pending = [&] {
    std::unique_lock<std::mutex> lk(mu);
    pending.emplace_back();
    cv.notify_one();
    return pending.back().get_future();
  };
  auto wait_pending = [&] {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return !pending.empty(); });
    auto p = std::move(pending.front());
    pending.pop_front();
    return p;
  };
  auto const expected_status = Status{StatusCode::kPermissionDenied, "uh-oh"};
  auto make_error_response = [&](int expected_count) {
    return [&, expected_count](
               google::cloud::CompletionQueue&,
               std::unique_ptr<grpc::ClientContext>,
               google::pubsub::v1::PublishRequest const& request) {
      EXPECT_EQ(expected_count, request.messages_size());
      return add_pending().then([&](future<void>) {
        return StatusOr<google::pubsub::v1::PublishResponse>(expected_status);
      });
    };
  };
  auto make_response = [&](int expected_count) {
    return
        [&, expected_count](google::cloud::CompletionQueue&,
                            std::unique_ptr<grpc::ClientContext>,
                            google::pubsub::v1::PublishRequest const& request) {
          EXPECT_EQ(expected_count, request.messages_size());
          google::pubsub::v1::PublishResponse response;
          for (auto const& m : request.messages()) {
            response.add_message_ids("id-" + std::string(m.data()));
          }
          return add_pending().then(
              [response](future<void>) { return make_status_or(response); });
        };
  };

  auto constexpr kBatchSize = 2;
  auto constexpr kDiscarded = 2 * kBatchSize;
  auto constexpr kResumed = 3 * kBatchSize;
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce(make_error_response(kBatchSize))
      .WillOnce(make_response(kBatchSize))
      .WillOnce(make_response(kResumed - kBatchSize));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(kBatchSize)
          .set_maximum_hold_time(std::chrono::seconds(5)),
      "test-key", mock, background.cq(), pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  std::vector<future<StatusOr<std::string>>> responses;
  for (int i = 0; i != kBatchSize + kDiscarded; ++i) {
    responses.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("d-" + std::to_string(i))
                                .SetOrderingKey("test-key")
                                .Build()}));
  }
  publisher->Flush({});

  // None of the responses should be ready because the mock has not sent a
  // response
  for (auto& r : responses) {
    using ms = std::chrono::milliseconds;
    EXPECT_EQ(std::future_status::timeout, r.wait_for(ms(0)));
  }
  // Trigger the first response. They should all fail with the error response.
  wait_pending().set_value();
  for (auto& r : responses) {
    EXPECT_THAT(
        r.get().status(),
        StatusIs(expected_status.code(), HasSubstr(expected_status.message())));
  }

  // Allow the publisher to publish again.
  publisher->ResumePublish({"test-key"});
  responses.clear();
  for (int i = 0; i != kResumed; ++i) {
    responses.push_back(
        publisher->Publish({pubsub::MessageBuilder{}
                                .SetData("d-" + std::to_string(i))
                                .SetOrderingKey("test-key")
                                .Build()}));
  }
  publisher->Flush({});

  // Trigger the following responses.
  wait_pending().set_value();  // First batch
  wait_pending().set_value();  // Corked batch
  for (auto& r : responses) {
    ASSERT_STATUS_OK(r.get());
  }
  // Cancel pending timer to speed up the test.
  background.cq().CancelAll();
}

TEST(BatchingPublisherConnectionTest, OrderingResetTimerOnCompletion) {
  pubsub::Topic const topic("test-project", "test-topic");

  std::mutex mu;
  std::condition_variable cv;
  std::deque<promise<void>> pending;
  auto add_pending = [&] {
    std::unique_lock<std::mutex> lk(mu);
    pending.emplace_back();
    cv.notify_one();
    return pending.back().get_future();
  };
  auto wait_pending = [&] {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [&] { return !pending.empty(); });
    auto p = std::move(pending.front());
    pending.pop_front();
    return p;
  };
  auto handle_async_push =
      [&](google::cloud::CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
          google::pubsub::v1::PublishRequest const& request) {
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("id-" + std::string(m.data()));
        }
        return add_pending().then(
            [response](future<void>) { return make_status_or(response); });
      };
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish).WillRepeatedly(handle_async_push);

  // Create an inactive queue, so no timers will ever "run", you can set them,
  // but they won't execute.
  CompletionQueue cq;
  auto constexpr kBatchSize = 4;
  auto const hold_time = std::chrono::milliseconds(5);
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_batch_message_count(kBatchSize)
          .set_maximum_hold_time(hold_time),
      "test-key", mock, cq, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());

  auto publish = [&](int count) {
    std::vector<future<StatusOr<std::string>>> r;
    for (int i = 0; i != count; ++i) {
      r.push_back(publisher->Publish({pubsub::MessageBuilder{}
                                          .SetData("d-" + std::to_string(i))
                                          .SetOrderingKey("test-key")
                                          .Build()}));
    }
    return r;
  };

  // Create a batch and flush it. Because we control when `AsyncPull()` succeeds
  // this batch will be pending and cork any future requests.
  auto r0 = publish(kBatchSize);
  publisher->Flush({});

  // Create a timer, because timers are sequenced, because CQ sequence the
  // timers this one will expire after any timers created by `publisher`.
  auto timer = cq.MakeRelativeTimer(hold_time);

  // Now create a partial batch. It is too small to flush something that would
  // not be sent by itself.
  auto r1 = publish(kBatchSize / 2);

  // Activate the queue, giving the timers a chance to run.
  std::thread t{[](CompletionQueue cq) { cq.Run(); }, cq};
  timer.get();

  // Trigger the first response. The responses should be successful.
  wait_pending().set_value();
  for (auto& r : r0) EXPECT_THAT(r.get().status(), StatusIs(StatusCode::kOk));

  // The partial batch should be sent eventually. That will unblock
  // wait_pending() and the remaining code.
  wait_pending().set_value();
  for (auto& r : r1) EXPECT_THAT(r.get().status(), StatusIs(StatusCode::kOk));

  cq.Shutdown();
  t.join();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
