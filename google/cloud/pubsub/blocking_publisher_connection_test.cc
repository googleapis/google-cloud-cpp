// Copyright 2022 Google LLC
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

#include "google/cloud/pubsub/blocking_publisher_connection.h"
#include "google/cloud/pubsub/internal/blocking_publisher_connection_impl.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/options.h"
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::HasSubstr;

std::shared_ptr<BlockingPublisherConnection> MakeTestPublisherConnection(
    std::shared_ptr<pubsub_internal::PublisherStub> mock, Options opts = {}) {
  opts = pubsub_internal::DefaultPublisherOptions(
      pubsub_testing::MakeTestOptions(std::move(opts)));
  return pubsub_internal::MakeTestBlockingPublisherConnection(
      std::move(opts), {std::move(mock)});
}

TEST(BlockingPublisherConnectionTest, Basic) {
  Topic const topic("test-project", "test-topic");

  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, Publish)
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ("test-data-0", request.messages(0).data());
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return response;
      });

  auto publisher = MakeTestPublisherConnection(mock);
  auto response = publisher->Publish(
      {topic, MessageBuilder{}.SetData("test-data-0").Build()});
  google::cloud::internal::OptionsSpan span(publisher->options());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ("test-message-id-0", *response);
}

TEST(BlockingPublisherConnectionTest, Metadata) {
  Topic const topic("test-project", "test-topic");

  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, Publish)
      .Times(AtLeast(1))
      .WillRepeatedly([&](grpc::ClientContext& context,
                          google::pubsub::v1::PublishRequest const& request) {
        google::cloud::testing_util::ValidateMetadataFixture fixture;
        fixture.IsContextMDValid(context, "google.pubsub.v1.Publisher.Publish",
                                 request,
                                 google::cloud::internal::ApiClientHeader());
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("ack-" + m.message_id());
        }
        return response;
      });

  auto publisher = MakeTestPublisherConnection(
      mock, Options{}.set<TracingComponentsOption>({"rpc"}));
  google::cloud::internal::OptionsSpan span(publisher->options());
  auto response = publisher->Publish(
      {topic, MessageBuilder{}.SetData("test-data-0").Build()});
  ASSERT_STATUS_OK(response);
}

TEST(BlockingPublisherConnectionTest, Logging) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  testing_util::ScopedLog log;

  EXPECT_CALL(*mock, Publish)
      .Times(AtLeast(1))
      .WillRepeatedly([&](grpc::ClientContext&,
                          google::pubsub::v1::PublishRequest const& request) {
        google::pubsub::v1::PublishResponse response;
        for (auto const& m : request.messages()) {
          response.add_message_ids("ack-" + m.message_id());
        }
        return response;
      });

  auto publisher = MakeTestPublisherConnection(
      mock, Options{}.set<TracingComponentsOption>({"rpc"}));
  google::cloud::internal::OptionsSpan span(publisher->options());
  auto response = publisher->Publish(
      {topic, MessageBuilder{}.SetData("test-data-0").Build()});
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("Publish")));
}

TEST(BlockingPublisherConnectionTest, HandlePermanentError) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, Publish)
      .WillOnce(
          [](grpc::ClientContext&, google::pubsub::v1::PublishRequest const&) {
            return Status(StatusCode::kPermissionDenied, "uh-oh");
          });

  auto publisher = MakeTestPublisherConnection(mock);
  auto response = publisher->Publish(
      {topic, MessageBuilder{}.SetData("test-message-0").Build()});
  EXPECT_THAT(response,
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

TEST(BlockingPublisherConnectionTest, HandleTooManyTransients) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, Publish)
      .WillRepeatedly(
          [](grpc::ClientContext&, google::pubsub::v1::PublishRequest const&) {
            return Status(StatusCode::kUnavailable, "try-again");
          });

  auto publisher = MakeTestPublisherConnection(mock);
  auto response = publisher->Publish(
      {topic, MessageBuilder{}.SetData("test-message-0").Build()});
  EXPECT_THAT(response,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(BlockingPublisherConnectionTest, HandleTransient) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, Publish)
      .WillOnce(
          [](grpc::ClientContext&, google::pubsub::v1::PublishRequest const&) {
            return Status(StatusCode::kUnavailable, "try-again");
          })
      .WillOnce(
          [](grpc::ClientContext&, google::pubsub::v1::PublishRequest const&) {
            google::pubsub::v1::PublishResponse response;
            response.add_message_ids("test-message-id");
            return response;
          });

  auto publisher = MakeTestPublisherConnection(mock);
  auto response = publisher->Publish(
      {topic, MessageBuilder{}.SetData("test-message-0").Build()});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(*response, "test-message-id");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
