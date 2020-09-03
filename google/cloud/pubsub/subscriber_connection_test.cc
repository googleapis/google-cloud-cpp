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

#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <atomic>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::HasSubstr;

TEST(SubscriberConnectionTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncPull(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PullRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        google::pubsub::v1::PullResponse response;
        auto& m = *response.add_received_messages();
        m.set_ack_id("test-ack-id-0");
        m.mutable_message()->set_message_id("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::AcknowledgeRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            EXPECT_FALSE(request.ack_ids().empty());
            for (auto const& id : request.ack_ids()) {
              EXPECT_EQ("test-ack-id-0", id);
            }
            return make_ready_future(Status{});
          });
  // Depending on timing this might be called, but it is very rare.
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  CompletionQueue cq;
  auto subscriber = pubsub_internal::MakeSubscriberConnection(
      mock,
      ConnectionOptions{grpc::InsecureChannelCredentials()}
          .DisableBackgroundThreads(cq),
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  std::atomic_flag received_one{false};
  promise<void> waiter;
  auto handler = [&](Message const& m, AckHandler h) {
    EXPECT_EQ("test-message-id-0", m.message_id());
    EXPECT_EQ("test-ack-id-0", h.ack_id());
    ASSERT_NO_FATAL_FAILURE(std::move(h).ack());
    if (received_one.test_and_set()) return;
    waiter.set_value();
  };
  std::thread t([&cq] { cq.Run(); });
  auto response = subscriber->Subscribe({subscription.FullName(), handler, {}});
  waiter.get_future().wait();
  response.cancel();
  ASSERT_STATUS_OK(response.get());
  // We need to explicitly cancel any pending timers (some of which may be quite
  // long) left by the subscription.
  cq.CancelAll();
  cq.Shutdown();
  t.join();
}

TEST(SubscriberConnectionTest, PullFailure) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  auto const expected = Status(StatusCode::kPermissionDenied, "uh-oh");
  EXPECT_CALL(*mock, AsyncPull(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PullRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        return make_ready_future(
            StatusOr<google::pubsub::v1::PullResponse>(expected));
      });

  auto subscriber = pubsub_internal::MakeSubscriberConnection(
      mock, ConnectionOptions{grpc::InsecureChannelCredentials()},
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  auto handler = [&](Message const&, AckHandler const&) {};
  auto response = subscriber->Subscribe({subscription.FullName(), handler, {}});
  EXPECT_THAT(response.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

/// @test Verify key events are logged
TEST(SubscriberConnectionTest, MakeSubscriberConnectionSetupsLogging) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncPull)
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PullRequest const&) {
        google::pubsub::v1::PullResponse response;
        auto& m = *response.add_received_messages();
        m.set_ack_id("test-ack-id-0");
        m.mutable_message()->set_message_id("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .Times(AtLeast(1))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  // Depending on timing this might be called, but it is very rare.
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  auto backend =
      std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  CompletionQueue cq;
  auto subscriber = pubsub_internal::MakeSubscriberConnection(
      mock,
      ConnectionOptions{grpc::InsecureChannelCredentials()}
          .DisableBackgroundThreads(cq)
          .enable_tracing("rpc"),
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  std::atomic_flag received_one{false};
  promise<void> waiter;
  auto handler = [&](Message const&, AckHandler h) {
    std::move(h).ack();
    if (received_one.test_and_set()) return;
    waiter.set_value();
  };
  std::thread t([&cq] { cq.Run(); });
  auto response = subscriber->Subscribe({subscription.FullName(), handler, {}});
  waiter.get_future().wait();
  response.cancel();
  ASSERT_STATUS_OK(response.get());
  // We need to explicitly cancel any pending timers (some of which may be quite
  // long) left by the subscription.
  cq.CancelAll();
  cq.Shutdown();
  t.join();

  auto const log_lines = backend->ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncPull")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncAcknowledge")));
  google::cloud::LogSink::Instance().RemoveBackend(id);
}

/// @test Verify the metadata decorator is configured
TEST(SubscriberConnectionTest, MakeSubscriberConnectionSetupsMetadata) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  Subscription const subscription("test-project", "test-subscription");

  EXPECT_CALL(*mock, AsyncPull)
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext> context,
                          google::pubsub::v1::PullRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(*context, "google.pubsub.v1.Subscriber.Pull",
                             google::cloud::internal::ApiClientHeader()));
        google::pubsub::v1::PullResponse response;
        auto& m = *response.add_received_messages();
        m.set_ack_id("test-ack-id-0");
        m.mutable_message()->set_message_id("test-message-id-0");
        return make_ready_future(make_status_or(response));
      });
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .Times(AtLeast(1))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  // Depending on timing this might be called, but it is very rare.
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  auto subscriber = pubsub_internal::MakeSubscriberConnection(
      mock, ConnectionOptions{grpc::InsecureChannelCredentials()},
      pubsub_testing::TestRetryPolicy(), pubsub_testing::TestBackoffPolicy());
  std::atomic_flag received_one{false};
  promise<void> waiter;
  auto handler = [&](Message const&, AckHandler h) {
    std::move(h).ack();
    if (received_one.test_and_set()) return;
    waiter.set_value();
  };
  auto response = subscriber->Subscribe({subscription.FullName(), handler, {}});
  waiter.get_future().wait();
  response.cancel();
  ASSERT_STATUS_OK(response.get());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
