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

#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/testing/mock_publisher_stub.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class PublisherLoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;

 private:
  long logger_id_ = 0;  // NOLINT(google-runtime-int)
};

TEST_F(PublisherLoggingTest, CreateTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, CreateTopic)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::Topic topic;
  topic.set_name("test-topic-name");
  auto status = stub.CreateTopic(context, topic);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(backend_->ClearLogLines(), Contains(HasSubstr("CreateTopic")));
}

TEST_F(PublisherLoggingTest, GetTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, GetTopic)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::GetTopicRequest request;
  request.set_topic("test-topic-name");
  auto status = stub.GetTopic(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(backend_->ClearLogLines(), Contains(HasSubstr("GetTopic")));
}

TEST_F(PublisherLoggingTest, UpdateTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, UpdateTopic)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Topic{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::UpdateTopicRequest request;
  request.mutable_topic()->set_name("test-topic-name");
  auto status = stub.UpdateTopic(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(backend_->ClearLogLines(), Contains(HasSubstr("UpdateTopic")));
}

TEST_F(PublisherLoggingTest, ListTopics) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopics)
      .WillOnce(
          Return(make_status_or(google::pubsub::v1::ListTopicsResponse{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicsRequest request;
  request.set_project("test-project-name");
  auto status = stub.ListTopics(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(
      backend_->ClearLogLines(),
      Contains(AllOf(HasSubstr("ListTopics"), HasSubstr("test-project-name"))));
}

TEST_F(PublisherLoggingTest, DeleteTopic) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, DeleteTopic).WillOnce(Return(Status{}));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::DeleteTopicRequest request;
  request.set_topic("test-topic-name");
  auto status = stub.DeleteTopic(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(
      backend_->ClearLogLines(),
      Contains(AllOf(HasSubstr("DeleteTopic"), HasSubstr("test-topic-name"))));
}

TEST_F(PublisherLoggingTest, DetachSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, DetachSubscription)
      .WillOnce(Return(
          make_status_or(google::pubsub::v1::DetachSubscriptionResponse{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::DetachSubscriptionRequest request;
  request.set_subscription("test-subscription-name");
  auto status = stub.DetachSubscription(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(backend_->ClearLogLines(),
              Contains(AllOf(HasSubstr("DetachSubscription"),
                             HasSubstr("test-subscription-name"))));
}

TEST_F(PublisherLoggingTest, ListTopicSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSubscriptions)
      .WillOnce(Return(make_status_or(
          google::pubsub::v1::ListTopicSubscriptionsResponse{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicSubscriptionsRequest request;
  request.set_topic("test-topic-name");
  auto status = stub.ListTopicSubscriptions(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(backend_->ClearLogLines(),
              Contains(AllOf(HasSubstr("ListTopicSubscriptions"),
                             HasSubstr("test-topic-name"))));
}

TEST_F(PublisherLoggingTest, ListTopicSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, ListTopicSnapshots)
      .WillOnce(Return(
          make_status_or(google::pubsub::v1::ListTopicSnapshotsResponse{})));
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::ListTopicSnapshotsRequest request;
  request.set_topic("test-topic-name");
  auto status = stub.ListTopicSnapshots(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(backend_->ClearLogLines(),
              Contains(AllOf(HasSubstr("ListTopicSnapshots"),
                             HasSubstr("test-topic-name"))));
}

TEST_F(PublisherLoggingTest, AsyncPublish) {
  auto mock = std::make_shared<pubsub_testing::MockPublisherStub>();
  EXPECT_CALL(*mock, AsyncPublish)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::PublishRequest const&) {
        return make_ready_future(
            make_status_or(google::pubsub::v1::PublishResponse{}));
      });
  PublisherLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::PublishRequest request;
  request.set_topic("test-topic-name");
  auto status =
      stub.AsyncPublish(cq, absl::make_unique<grpc::ClientContext>(), request)
          .get();
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(
      backend_->ClearLogLines(),
      Contains(AllOf(HasSubstr("AsyncPublish"), HasSubstr("test-topic-name"))));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
