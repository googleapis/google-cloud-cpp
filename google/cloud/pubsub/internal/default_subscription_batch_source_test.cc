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

#include "google/cloud/pubsub/internal/default_subscription_batch_source.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads;
using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;

TEST(DefaultSubscriptionBatchSourceTest, AckMessage) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status(StatusCode::kUnavailable, "try-again"));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::AcknowledgeRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        EXPECT_THAT(request.ack_ids(), ElementsAre("test-ack-01"));
        return make_ready_future(Status{});
      });

  AutomaticallyCreatedBackgroundThreads background;
  DefaultSubscriptionBatchSource uut(background.cq(), mock,
                                     subscription.FullName(), TestRetryPolicy(),
                                     TestBackoffPolicy());
  auto result = uut.AckMessage("test-ack-01", 0);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kOk));
}

TEST(DefaultSubscriptionBatchSourceTest, NackMessage) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status(StatusCode::kUnavailable, "try-again"));
      })
      .WillOnce(
          [&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            EXPECT_THAT(request.ack_ids(), ElementsAre("test-ack-01"));
            EXPECT_EQ(0, request.ack_deadline_seconds());
            return make_ready_future(Status{});
          });

  AutomaticallyCreatedBackgroundThreads background;
  DefaultSubscriptionBatchSource uut(background.cq(), mock,
                                     subscription.FullName(), TestRetryPolicy(),
                                     TestBackoffPolicy());
  auto result = uut.NackMessage("test-ack-01", 0);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kOk));
}

TEST(DefaultSubscriptionBatchSourceTest, BulkNack) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status(StatusCode::kUnavailable, "try-again"));
      })
      .WillOnce(
          [&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            EXPECT_THAT(
                request.ack_ids(),
                ElementsAre("test-ack-01", "test-ack-02", "test-ack-03"));
            EXPECT_EQ(0, request.ack_deadline_seconds());
            return make_ready_future(Status{});
          });

  AutomaticallyCreatedBackgroundThreads background;
  DefaultSubscriptionBatchSource uut(background.cq(), mock,
                                     subscription.FullName(), TestRetryPolicy(),
                                     TestBackoffPolicy());
  auto result = uut.BulkNack({"test-ack-01", "test-ack-02", "test-ack-03"}, 0);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kOk));
}

TEST(DefaultSubscriptionBatchSourceTest, ExtendLeases) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status(StatusCode::kUnavailable, "try-again"));
      })
      .WillOnce(
          [&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            EXPECT_THAT(
                request.ack_ids(),
                ElementsAre("test-ack-01", "test-ack-02", "test-ack-03"));
            EXPECT_EQ(123, request.ack_deadline_seconds());
            return make_ready_future(Status{});
          })
      .WillOnce(
          [&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            EXPECT_THAT(request.ack_ids(), ElementsAre("test-ack-04"));
            EXPECT_EQ(0, request.ack_deadline_seconds());
            return make_ready_future(Status{});
          })
      .WillOnce(
          [&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            EXPECT_THAT(request.ack_ids(), ElementsAre("test-ack-05"));
            EXPECT_EQ(600, request.ack_deadline_seconds());
            return make_ready_future(Status{});
          });

  AutomaticallyCreatedBackgroundThreads background;
  DefaultSubscriptionBatchSource uut(background.cq(), mock,
                                     subscription.FullName(), TestRetryPolicy(),
                                     TestBackoffPolicy());
  auto result = uut.ExtendLeases({"test-ack-01", "test-ack-02", "test-ack-03"},
                                 std::chrono::seconds(123));
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kOk));

  result = uut.ExtendLeases({"test-ack-04"}, std::chrono::seconds(-12));
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kOk));

  result = uut.ExtendLeases({"test-ack-05"}, std::chrono::seconds(1234));
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kOk));
}

TEST(DefaultSubscriptionBatchSourceTest, Pull) {
  auto subscription = pubsub::Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();

  auto constexpr kText = R"pb(
    received_messages {
      message {
        data: "m0"
        attributes: { key: "k0" value: "m0-l0" }
        attributes: { key: "k1" value: "m0-l1" }
        message_id: "id-m0"
        ordering_key: "abcd"
      }
      ack_id: "ack-m0"
    }
    received_messages {
      message {
        data: "m1"
        attributes: { key: "k0" value: "m1-l0" }
        attributes: { key: "k1" value: "m1-l1" }
        message_id: "id-m1"
        ordering_key: "abcd"
      }
      ack_id: "ack-m1"
    })pb";
  google::pubsub::v1::PullResponse response;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  EXPECT_CALL(*mock, AsyncPull)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PullRequest const&) {
        return make_ready_future(StatusOr<google::pubsub::v1::PullResponse>(
            Status(StatusCode::kUnavailable, "try-again")));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::pubsub::v1::PullRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        EXPECT_EQ(42, request.max_messages());
        return make_ready_future(make_status_or(response));
      });

  AutomaticallyCreatedBackgroundThreads background;
  DefaultSubscriptionBatchSource uut(background.cq(), mock,
                                     subscription.FullName(), TestRetryPolicy(),
                                     TestBackoffPolicy());
  auto result = uut.Pull(42).get();
  ASSERT_THAT(result, StatusIs(StatusCode::kOk));
  EXPECT_THAT(result.value(), IsProtoEqual(response));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
