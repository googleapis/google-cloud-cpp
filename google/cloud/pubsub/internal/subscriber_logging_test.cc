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

#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
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

class SubscriberLoggingTest : public ::testing::Test {
 protected:
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
};

TEST_F(SubscriberLoggingTest, CreateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSubscription)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Subscription{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::Subscription subscription;
  auto status = stub.CreateSubscription(context, subscription);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateSubscription")));
}

TEST_F(SubscriberLoggingTest, GetSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSubscription)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Subscription{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::GetSubscriptionRequest request;
  auto status = stub.GetSubscription(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetSubscription")));
}

TEST_F(SubscriberLoggingTest, UpdateSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSubscription)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Subscription{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::UpdateSubscriptionRequest request;
  auto status = stub.UpdateSubscription(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("UpdateSubscription")));
}

TEST_F(SubscriberLoggingTest, ListSubscriptions) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSubscriptions)
      .WillOnce(Return(
          make_status_or(google::pubsub::v1::ListSubscriptionsResponse{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::ListSubscriptionsRequest request;
  request.set_project("test-project-name");
  auto status = stub.ListSubscriptions(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("ListSubscriptions"),
                             HasSubstr("test-project-name"))));
}

TEST_F(SubscriberLoggingTest, DeleteSubscription) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSubscription).WillOnce(Return(Status{}));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSubscriptionRequest request;
  request.set_subscription("test-subscription-name");
  auto status = stub.DeleteSubscription(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("DeleteSubscription"),
                             HasSubstr("test-subscription-name"))));
}

TEST_F(SubscriberLoggingTest, ModifyPushConfig) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ModifyPushConfig).WillOnce(Return(Status{}));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::ModifyPushConfigRequest request;
  request.set_subscription("test-subscription-name");
  auto status = stub.ModifyPushConfig(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("ModifyPushConfig"),
                             HasSubstr("test-subscription-name"))));
}

TEST_F(SubscriberLoggingTest, AsyncStreamingPull) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::StreamingPullRequest const&) {
        auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Cancel).Times(1);
        EXPECT_CALL(*stream, Start).WillOnce([&] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([&] {
              return make_ready_future(absl::make_optional(
                  google::pubsub::v1::StreamingPullResponse{}));
            })
            .WillOnce([&] {
              return make_ready_future(
                  absl::optional<google::pubsub::v1::StreamingPullResponse>{});
            });
        EXPECT_CALL(*stream, Write)
            .WillOnce([&](google::pubsub::v1::StreamingPullRequest const&,
                          grpc::WriteOptions const&) {
              return make_ready_future(true);
            });
        EXPECT_CALL(*stream, WritesDone).WillOnce([&] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([&] {
          return make_ready_future(Status{});
        });
        return stream;
      });
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         true);
  google::cloud::CompletionQueue cq;

  google::pubsub::v1::StreamingPullRequest request;
  request.set_subscription("test-subscription-name");
  auto stream = stub.AsyncStreamingPull(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("AsyncStreamingPull")));

  EXPECT_TRUE(stream->Start().get());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("Start")));

  EXPECT_TRUE(
      stream->Write(request, grpc::WriteOptions{}.set_write_through()).get());
  EXPECT_THAT(
      log_.ExtractLines(),
      Contains(AllOf(HasSubstr("Write"), HasSubstr("test-subscription-name"))));

  EXPECT_TRUE(stream->Read().get().has_value());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("Read")));

  EXPECT_FALSE(stream->Read().get().has_value());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("Read")));

  EXPECT_TRUE(stream->WritesDone().get());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("WritesDone")));

  EXPECT_STATUS_OK(stream->Finish().get());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("Finish")));

  stream->Cancel();
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("Cancel")));
}

TEST_F(SubscriberLoggingTest, AsyncAcknowledge) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::AcknowledgeRequest request;
  request.set_subscription("test-subscription-name");
  auto status = stub.AsyncAcknowledge(
                        cq, absl::make_unique<grpc::ClientContext>(), request)
                    .get();
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("AsyncAcknowledge"),
                             HasSubstr("test-subscription-name"))));
}

TEST_F(SubscriberLoggingTest, AsyncModifyAckDeadline) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  google::cloud::CompletionQueue cq;
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription("test-subscription-name");
  auto status = stub.AsyncModifyAckDeadline(
                        cq, absl::make_unique<grpc::ClientContext>(), request)
                    .get();
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("AsyncModifyAckDeadline"),
                             HasSubstr("test-subscription-name"))));
}

TEST_F(SubscriberLoggingTest, CreateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, CreateSnapshot)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Snapshot{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::CreateSnapshotRequest request;
  auto status = stub.CreateSnapshot(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateSnapshot")));
}

TEST_F(SubscriberLoggingTest, GetSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, GetSnapshot)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Snapshot{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::GetSnapshotRequest request;
  auto status = stub.GetSnapshot(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetSnapshot")));
}

TEST_F(SubscriberLoggingTest, ListSnapshots) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, ListSnapshots)
      .WillOnce(
          Return(make_status_or(google::pubsub::v1::ListSnapshotsResponse{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::ListSnapshotsRequest request;
  request.set_project("test-project-name");
  auto status = stub.ListSnapshots(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("ListSnapshots"),
                             HasSubstr("test-project-name"))));
}

TEST_F(SubscriberLoggingTest, UpdateSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, UpdateSnapshot)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Snapshot{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::UpdateSnapshotRequest request;
  auto status = stub.UpdateSnapshot(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("UpdateSnapshot")));
}

TEST_F(SubscriberLoggingTest, DeleteSnapshot) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, DeleteSnapshot).WillOnce(Return(Status{}));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSnapshotRequest request;
  auto status = stub.DeleteSnapshot(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteSnapshot")));
}

TEST_F(SubscriberLoggingTest, Seek) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, Seek)
      .WillOnce(Return(make_status_or(google::pubsub::v1::SeekResponse{})));
  SubscriberLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"),
                         false);
  grpc::ClientContext context;
  google::pubsub::v1::SeekRequest request;
  request.set_subscription("test-subscription-name");
  auto status = stub.Seek(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(
      log_.ExtractLines(),
      Contains(AllOf(HasSubstr("Seek"), HasSubstr("test-subscription-name"))));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
