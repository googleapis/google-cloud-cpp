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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <condition_variable>
#include <deque>
#include <mutex>

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
      sizeof("test-data-N") + kMessageSizeOverhead + 2;
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

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto const ordering_key = std::string{};
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::PublisherOptions{}
          .set_maximum_hold_time(std::chrono::milliseconds(5))
          .set_maximum_batch_message_count(4),
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

TEST(BatchingPublisherConnectionTest, OrderingBatchRejectAfterError) {
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
  // Trigger the first response.
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
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
