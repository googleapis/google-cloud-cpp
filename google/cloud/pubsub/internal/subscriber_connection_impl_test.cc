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

#include "google/cloud/pubsub/internal/subscriber_connection_impl.h"
#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/testing/fake_streaming_pull.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <atomic>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub::AckHandler;
using ::google::cloud::pubsub::ExactlyOnceAckHandler;
using ::google::cloud::pubsub::Message;
using ::google::cloud::pubsub::Subscription;
using ::google::cloud::pubsub_testing::FakeAsyncStreamingPull;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::StartsWith;

Options MakeTestOptions(Options opts = {}) {
  opts.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  opts = pubsub_internal::DefaultSubscriberOptions(
      pubsub_testing::MakeTestOptions(std::move(opts)));
  // The CI scripts set an environment variable that overrides this option. We
  // are not interested in this behavior for this test.
  opts.unset<google::cloud::UserProjectOption>();
  return opts;
}

Options MakeTestOptions(CompletionQueue const& cq) {
  return MakeTestOptions(Options{}.set<GrpcCompletionQueueOption>(cq));
}

TEST(SubscriberConnectionTest, Basic) {
  auto const subscription = Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::AcknowledgeRequest const& request) {
        EXPECT_THAT(request.ack_ids(), Contains("test-ack-id-0"));
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);

  CompletionQueue cq;
  auto subscriber = std::make_shared<SubscriberConnectionImpl>(
      subscription, MakeTestOptions(cq), mock);
  std::atomic_flag received_one{false};
  promise<void> waiter;
  auto handler = [&](Message const& m, AckHandler h) {
    if (received_one.test_and_set()) return;
    EXPECT_THAT(m.message_id(), StartsWith("test-message-id-"));
    ASSERT_NO_FATAL_FAILURE(std::move(h).ack());
    waiter.set_value();
  };
  std::thread t([&cq] { cq.Run(); });
  google::cloud::internal::OptionsSpan span(subscriber->options());
  auto response = subscriber->Subscribe({handler});
  waiter.get_future().wait();
  response.cancel();
  ASSERT_STATUS_OK(response.get());
  // We need to explicitly cancel any pending timers (some of which may be quite
  // long) left by the subscription.
  cq.CancelAll();
  cq.Shutdown();
  t.join();
}

TEST(SubscriberConnectionTest, ExactlyOnce) {
  auto const subscription = Subscription("test-project", "test-subscription");
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillOnce([](google::cloud::CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext>,
                   google::pubsub::v1::AcknowledgeRequest const& request) {
        EXPECT_THAT(request.ack_ids(), Contains("test-ack-id-0"));
        return make_ready_future(
            Status{StatusCode::kUnknown, "test-only-unknown"});
      });
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly(FakeAsyncStreamingPull);

  CompletionQueue cq;
  auto subscriber = std::make_shared<SubscriberConnectionImpl>(
      subscription, MakeTestOptions(cq), mock);
  std::atomic_flag received_one{false};
  promise<void> waiter;
  auto callback = [&](Message const& m, ExactlyOnceAckHandler h) {
    if (received_one.test_and_set()) return;
    EXPECT_THAT(m.message_id(), StartsWith("test-message-id-"));
    auto status = std::move(h).ack().get();
    EXPECT_THAT(status, StatusIs(StatusCode::kUnknown, "test-only-unknown"));
    waiter.set_value();
  };
  std::thread t([&cq] { cq.Run(); });
  google::cloud::internal::OptionsSpan span(subscriber->options());
  auto response = subscriber->ExactlyOnceSubscribe({callback});
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
  auto const subscription = Subscription("test-project", "test-subscription");
  auto const expected = Status(StatusCode::kPermissionDenied, "uh-oh");

  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncStreamingPull)
      .Times(AtLeast(1))
      .WillRepeatedly([](google::cloud::CompletionQueue const& cq,
                         std::unique_ptr<grpc::ClientContext>) {
        using TimerFuture =
            future<StatusOr<std::chrono::system_clock::time_point>>;
        using us = std::chrono::microseconds;

        auto start_response = [q = cq]() mutable {
          return q.MakeRelativeTimer(us(10)).then(
              [](TimerFuture) { return false; });
        };
        auto finish_response = [q = cq]() mutable {
          return q.MakeRelativeTimer(us(10)).then([](TimerFuture) {
            return Status{StatusCode::kPermissionDenied, "uh-oh"};
          });
        };

        auto stream = absl::make_unique<pubsub_testing::MockAsyncPullStream>();
        EXPECT_CALL(*stream, Start).WillOnce(start_response);
        EXPECT_CALL(*stream, Finish).WillOnce(finish_response);
        return stream;
      });

  auto subscriber = std::make_shared<SubscriberConnectionImpl>(
      subscription, pubsub_testing::MakeTestOptions(), mock);
  auto handler = [&](Message const&, AckHandler const&) {};
  google::cloud::internal::OptionsSpan span(subscriber->options());
  auto response = subscriber->Subscribe({handler});
  EXPECT_THAT(response.get(),
              StatusIs(StatusCode::kPermissionDenied, HasSubstr("uh-oh")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
