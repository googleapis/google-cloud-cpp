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

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::HasSubstr;

TEST(PublisherConnectionTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, {}, {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("test-message-id-0", *response);
}

TEST(PublisherConnectionTest, Metadata) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext> context,
                          google::pubsub::v1::PublishRequest const& request) {
        EXPECT_STATUS_OK(google::cloud::testing_util::IsContextMDValid(
            *context, "google.pubsub.v1.Publisher.Publish",
            google::cloud::internal::ApiClientHeader()));
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("ack-" + m.message_id());
        }
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, {}, ConnectionOptions{}.enable_tracing("rpc"), mock,
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  ASSERT_STATUS_OK(response);
}

TEST(PublisherConnectionTest, Logging) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  testing_util::ScopedLog log;

  EXPECT_CALL(*mock, AsyncPublish)
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const& request) {
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("ack-" + m.message_id());
        }
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, {}, ConnectionOptions{}.enable_tracing("rpc"), mock,
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("AsyncPublish")));
}

TEST(PublisherConnectionTest, OrderingKey) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, PublisherOptions{}.enable_message_ordering(), {}, mock,
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("test-message-id-0", *response);
}

TEST(PublisherConnectionTest, OrderingKeyWithoutMessageOrdering) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, PublisherOptions{}, {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response = publisher
                      ->Publish({MessageBuilder{}
                                     .SetOrderingKey("test-ordering-key-0")
                                     .SetData("test-data-0")
                                     .Build()})
                      .get();
  EXPECT_EQ(StatusCode::kInvalidArgument, response.status().code());
  EXPECT_THAT(response.status().message(),
              HasSubstr("does not have message ordering enabled"));
}

TEST(PublisherConnectionTest, HandleInvalidResponse) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const&) {
        google::pubsub::v1::PublishResponse response;
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, {}, {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  // It is very unlikely we will see this in production, it would indicate a bug
  // in the Cloud Pub/Sub service where we successfully published N events, but
  // we received M != N message ids back.
  EXPECT_EQ(StatusCode::kUnknown, response.status().code());
  EXPECT_THAT(response.status().message(),
              HasSubstr("mismatched message id count"));
}

TEST(PublisherConnectionTest, HandleTooManyFailures) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .Times(AtLeast(2))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status(StatusCode::kUnavailable, "try-again")));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, PublisherOptions{}, {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-message-0").Build()})
          .get();
  EXPECT_THAT(response,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(PublisherConnectionTest, HandlePermanentError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, {}, {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-message-0").Build()})
          .get();
  EXPECT_THAT(response,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(PublisherConnectionTest, HandleTransientDisabledRetry) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status(StatusCode::kUnavailable, "try-again")));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, PublisherOptions{}, {}, mock,
      pubsub::LimitedErrorCountRetryPolicy(/*maximum_failures=*/0).clone(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-message-0").Build()})
          .get();
  EXPECT_THAT(response,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(PublisherConnectionTest, HandleTransientEnabledRetry) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(StatusOr<google::pubsub::v1::PublishResponse>(
            Status(StatusCode::kUnavailable, "try-again")));
      })
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      topic, PublisherOptions{}, {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({MessageBuilder{}.SetData("test-data-0").Build()})
          .get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("test-message-id-0", *response);
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
