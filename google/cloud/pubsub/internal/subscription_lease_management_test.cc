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

#include "google/cloud/pubsub/internal/subscription_lease_management.h"
#include "google/cloud/pubsub/testing/mock_subscription_batch_source.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;

google::pubsub::v1::PullResponse MakePullResponse(std::string const& prefix,
                                                  int count) {
  google::pubsub::v1::PullResponse response;
  for (int i = 0; i != count; ++i) {
    auto const id = prefix + std::to_string(i);
    auto& m = *response.add_received_messages();
    m.set_ack_id("ack-" + id);
    m.mutable_message()->set_message_id("message-" + id);
  }
  return response;
}

future<Status> SimpleAckNack(std::string const&, std::size_t) {
  return make_ready_future(Status{});
}

TEST(SubscriptionLeaseManagementTest, NormalLifecycle) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();

  auto constexpr kTestDeadline = std::chrono::seconds(345);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, Pull(3)).WillOnce([&](std::int32_t) {
      return make_ready_future(make_status_or(MakePullResponse("0-", 3)));
    });
    // We expect this message to be acked.
    EXPECT_CALL(*mock, AckMessage("ack-0-1", _)).WillOnce(SimpleAckNack);
    EXPECT_CALL(*mock, ExtendLeases)
        // Then a simulated timer refreshes the lease for the remaining
        // messages.
        .WillOnce([&](std::vector<std::string> const& ack_ids,
                      std::chrono::seconds extension) {
          EXPECT_THAT(ack_ids, ElementsAre("ack-0-0", "ack-0-2"));
          EXPECT_LE(std::abs((kTestDeadline - extension).count()), 2);
          return make_ready_future(Status{});
        });
    // Then a message is nacked.
    EXPECT_CALL(*mock, NackMessage("ack-0-2", _)).WillOnce(SimpleAckNack);
    EXPECT_CALL(*mock, ExtendLeases)
        // The a simulated timer refreshes the leases.
        .WillOnce([&](std::vector<std::string> const& ack_ids,
                      std::chrono::seconds extension) {
          EXPECT_THAT(ack_ids, ElementsAre("ack-0-0"));
          EXPECT_LE(std::abs((kTestDeadline - extension).count()), 2);
          return make_ready_future(Status{});
        });
    // Then all unhandled messages are nacked on shutdown.
    EXPECT_CALL(*mock, BulkNack(ElementsAre("ack-0-0"), _))
        .WillOnce([](std::vector<std::string> const&, std::size_t) {
          return make_ready_future(Status{});
        });
    EXPECT_CALL(*mock, Shutdown).Times(1);
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  std::vector<promise<Status>> timers;
  int cancel_count = 0;
  auto make_timer = [&](std::chrono::system_clock::time_point) {
    promise<Status> p([&cancel_count] { ++cancel_count; });
    auto f = p.get_future().then([](future<Status> f) { return f.get(); });
    timers.push_back(std::move(p));
    return f;
  };
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::CreateForTesting(
      background.cq(), shutdown_manager, make_timer, mock,
      std::chrono::seconds(kTestDeadline));

  auto acks = [](google::pubsub::v1::PullResponse const& response) {
    std::vector<std::string> acks;
    std::transform(response.received_messages().begin(),
                   response.received_messages().end(), std::back_inserter(acks),
                   [](google::pubsub::v1::ReceivedMessage const& m) {
                     return m.ack_id();
                   });
    return acks;
  };

  auto done = shutdown_manager->Start({});
  auto response = uut->Pull(3).get();
  ASSERT_THAT(response.status(), StatusIs(StatusCode::kOk));
  EXPECT_THAT(acks(*response), ElementsAre("ack-0-0", "ack-0-1", "ack-0-2"));
  ASSERT_EQ(1, timers.size());

  // Ack one of the messages and then fire the timer. The expectations set above
  // will verify that only the remaining messages have their lease extended.
  uut->AckMessage("ack-0-1", 0);
  timers[0].set_value({});
  EXPECT_THAT(2, timers.size());

  // Ack one more message and trigger the new timer.
  uut->NackMessage("ack-0-2", 0);
  timers[1].set_value({});
  EXPECT_THAT(3, timers.size());

  shutdown_manager->MarkAsShutdown(__func__, Status{});
  uut->Shutdown();
  EXPECT_EQ(3, cancel_count);
  timers[2].set_value(Status(StatusCode::kCancelled, "test-cancel"));
  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

TEST(SubscriptionLeaseManagementTest, ShutdownOnError) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();

  auto constexpr kTestDeadline = 345;
  EXPECT_CALL(*mock, Pull(3)).WillOnce([&](std::int32_t) {
    return make_ready_future(make_status_or(MakePullResponse("0-", 3)));
  });
  EXPECT_CALL(*mock, Pull(4)).WillOnce([&](std::int32_t) {
    return make_ready_future(StatusOr<google::pubsub::v1::PullResponse>(
        Status(StatusCode::kPermissionDenied, "uh-oh")));
  });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  std::vector<promise<Status>> timers;
  int cancel_count = 0;
  auto make_timer = [&](std::chrono::system_clock::time_point) {
    promise<Status> p([&cancel_count] { ++cancel_count; });
    auto f = p.get_future().then([](future<Status> f) { return f.get(); });
    timers.push_back(std::move(p));
    return f;
  };
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::CreateForTesting(
      background.cq(), shutdown_manager, make_timer, mock,
      std::chrono::seconds(kTestDeadline));

  auto done = shutdown_manager->Start({});
  auto response = uut->Pull(3).get();
  ASSERT_THAT(response.status(), StatusIs(StatusCode::kOk));
  ASSERT_EQ(1, timers.size());

  response = uut->Pull(4).get();
  ASSERT_THAT(response.status(), StatusIs(StatusCode::kPermissionDenied));
  ASSERT_EQ(1, timers.size());

  timers[0].set_value(Status(StatusCode::kCancelled, "test-cancel"));
  EXPECT_THAT(done.get(), StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify ExtendLeases does not call the `child` object.
TEST(SubscriptionLeaseManagementTest, DoesNotPropagateExtendLeases) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::Create(
      background.cq(), shutdown_manager, mock, std::chrono::seconds(30));

  auto result =
      uut->ExtendLeases({"a", "b", "c"}, std::chrono::seconds(10)).get();
  ASSERT_THAT(result, StatusIs(StatusCode::kUnimplemented));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
