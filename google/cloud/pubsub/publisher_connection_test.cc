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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
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
using ::testing::ElementsAre;
using ::testing::HasSubstr;

std::vector<std::string> DataElements(
    google::pubsub::v1::PublishRequest const& request) {
  std::vector<std::string> data;
  std::transform(request.messages().begin(), request.messages().end(),
                 std::back_inserter(data),
                 [](google::pubsub::v1::PubsubMessage const& m) {
                   return std::string(m.data());
                 });
  return data;
}

TEST(PublisherConnectionTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");

  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([&](google::cloud::CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PublishRequest const& request) {
        EXPECT_EQ(topic.FullName(), request.topic());
        EXPECT_THAT(DataElements(request), ElementsAre("test-data-0"));
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  google::pubsub::v1::PublishRequest request;
  request.set_topic(topic.FullName());
  request.add_messages()->set_data("test-data-0");
  auto response = publisher->Publish({std::move(request)}).get();
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(response->message_ids(), ElementsAre("test-message-id-0"));
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
      ConnectionOptions{}.enable_tracing("rpc"), mock,
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  google::pubsub::v1::PublishRequest request;
  request.set_topic(topic.FullName());
  request.add_messages()->set_data("test-data-0");
  auto response = publisher->Publish({std::move(request)}).get();
  ASSERT_STATUS_OK(response);
}

TEST(PublisherConnectionTest, Logging) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  Topic const topic("test-project", "test-topic");
  auto backend =
      std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

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
      ConnectionOptions{}.enable_tracing("rpc"), mock,
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  google::pubsub::v1::PublishRequest request;
  request.add_messages()->set_data("test-data-0");
  auto response = publisher->Publish({std::move(request)}).get();
  ASSERT_STATUS_OK(response);

  EXPECT_THAT(backend->ClearLogLines(), Contains(HasSubstr("AsyncPublish")));
  google::cloud::LogSink::Instance().RemoveBackend(id);
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
      {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({google::pubsub::v1::PublishRequest{}}).get();
  EXPECT_THAT(response.status(),
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
      {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({google::pubsub::v1::PublishRequest{}}).get();
  EXPECT_THAT(response.status(),
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
      {}, mock,
      pubsub::LimitedErrorCountRetryPolicy(/*maximum_failures=*/0).clone(),
      pubsub_testing::TestBackoffPolicy());
  auto response =
      publisher->Publish({google::pubsub::v1::PublishRequest{}}).get();
  EXPECT_THAT(response.status(),
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
        EXPECT_THAT(DataElements(request), ElementsAre("test-data-0"));
        google::pubsub::v1::PublishResponse response;
        response.add_message_ids("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });

  auto publisher = pubsub_internal::MakePublisherConnection(
      {}, mock, pubsub_testing::TestRetryPolicy(),
      pubsub_testing::TestBackoffPolicy());
  google::pubsub::v1::PublishRequest request;
  request.set_topic(topic.FullName());
  request.add_messages()->set_data("test-data-0");
  auto response = publisher->Publish({std::move(request)}).get();
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(response->message_ids(), ElementsAre("test-message-id-0"));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
