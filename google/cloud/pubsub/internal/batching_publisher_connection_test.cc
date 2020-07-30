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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/mock_completion_queue.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

TEST(BatchingPublisherConnectionTest, DefaultMakesProgress) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
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

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads bg;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::BatchingConfig{}
          .set_maximum_message_count(4)
          .set_maximum_hold_time(std::chrono::milliseconds(50)),
      mock, bg.cq());

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

  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
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

  // Use our own completion queue, initially inactive, to avoid race conditions
  // due to the zero-maximum-hold-time timer expiring.
  google::cloud::CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::BatchingConfig{}.set_maximum_message_count(2), mock, cq);
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

  std::thread t([&cq] { cq.Run(); });

  r0.get();
  r1.get();

  cq.Shutdown();
  t.join();
}

TEST(BatchingPublisherConnectionTest, BatchByMessageSize) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
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

  auto constexpr kMaxMessageBytes = sizeof("test-data-N") + 2;
  // Use our own completion queue, initially inactive, to avoid race conditions
  // due to the zero-maximum-hold-time timer expiring.
  google::cloud::CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::BatchingConfig{}
          .set_maximum_message_count(4)
          .set_maximum_batch_bytes(kMaxMessageBytes),
      mock, cq);
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

  std::thread t([&cq] { cq.Run(); });

  r0.get();
  r1.get();

  cq.Shutdown();
  t.join();
}

TEST(BatchingPublisherConnectionTest, BatchByMaximumHoldTime) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
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

  // Use our own completion queue, initially inactive, to avoid race conditions
  // due to the maximum-hold-time timer expiring.
  google::cloud::CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::BatchingConfig{}
          .set_maximum_hold_time(std::chrono::milliseconds(5))
          .set_maximum_message_count(4),
      mock, cq);
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

  std::thread t([&cq] { cq.Run(); });

  r0.get();
  r1.get();

  cq.Shutdown();
  t.join();
}

TEST(BatchingPublisherConnectionTest, BatchByFlush) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
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

  // Use our own completion queue, initially inactive, to avoid race conditions
  // due to the maximum-hold-time timer expiring.
  google::cloud::CompletionQueue cq;
  auto publisher = BatchingPublisherConnection::Create(
      topic,
      pubsub::BatchingConfig{}
          .set_maximum_hold_time(std::chrono::milliseconds(5))
          .set_maximum_message_count(4),
      mock, cq);

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

  std::thread t([&cq] { cq.Run(); });
  for (auto& r : results) r.get();
  cq.Shutdown();
  t.join();
}

TEST(BatchingPublisherConnectionTest, HandleError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  pubsub::Topic const topic("test-project", "test-topic");

  auto const error_status = Status(StatusCode::kPermissionDenied, "uh-oh");
  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(
            StatusOr<google::pubsub::v1::PublishResponse>(error_status));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads bg;
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::BatchingConfig{}.set_maximum_message_count(2), mock,
      bg.cq());
  auto check_status = [&](future<StatusOr<std::string>> f) {
    auto r = f.get();
    EXPECT_EQ(error_status, r.status());
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

  EXPECT_CALL(*mock, AsyncPublish(_, _, _))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const&) {
        google::pubsub::v1::PublishResponse response;
        return make_ready_future(make_status_or(response));
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads bg;
  auto publisher = BatchingPublisherConnection::Create(
      topic, pubsub::BatchingConfig{}.set_maximum_message_count(2), mock,
      bg.cq());
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

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
