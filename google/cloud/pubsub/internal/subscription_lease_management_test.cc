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
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::UnorderedElementsAre;

google::pubsub::v1::StreamingPullResponse GenerateMessages(
    std::string const& prefix, int count) {
  google::pubsub::v1::StreamingPullResponse response;
  for (int i = 0; i != count; ++i) {
    auto const id = prefix + std::to_string(i);
    auto& m = *response.add_received_messages();
    m.set_ack_id("ack-" + id);
    m.mutable_message()->set_message_id("message-" + id);
  }
  return response;
}

future<Status> SimpleAckNack(std::string const&) {
  return make_ready_future(Status{});
}

TEST(SubscriptionLeaseManagementTest, NormalLifecycle) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  auto constexpr kTestDeadline = std::chrono::seconds(345);
  {
    ::testing::InSequence sequence;
    // We expect this message to be acked.
    EXPECT_CALL(*mock, AckMessage("ack-0-1")).WillOnce(SimpleAckNack);
    EXPECT_CALL(*mock, ExtendLeases)
        // Then a simulated timer refreshes the lease for the remaining
        // messages.
        .WillOnce([&](std::vector<std::string> const& ack_ids,
                      std::chrono::seconds extension) {
          EXPECT_THAT(ack_ids, UnorderedElementsAre("ack-0-0", "ack-0-2"));
          EXPECT_LE(std::abs((kTestDeadline - extension).count()), 2);
          return make_ready_future(Status{});
        });
    // Then a message is nacked.
    EXPECT_CALL(*mock, NackMessage("ack-0-2")).WillOnce(SimpleAckNack);
    EXPECT_CALL(*mock, ExtendLeases)
        // The a simulated timer refreshes the leases.
        .WillOnce([&](std::vector<std::string> const& ack_ids,
                      std::chrono::seconds extension) {
          EXPECT_THAT(ack_ids, UnorderedElementsAre("ack-0-0"));
          EXPECT_LE(std::abs((kTestDeadline - extension).count()), 2);
          return make_ready_future(Status{});
        });
    // Then all unhandled messages are nacked on shutdown.
    EXPECT_CALL(*mock, BulkNack(UnorderedElementsAre("ack-0-0")))
        .WillOnce([](std::vector<std::string> const&) {
          return make_ready_future(Status{});
        });
    EXPECT_CALL(*mock, Shutdown).Times(1);
  }

  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(fake_cq);

  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::Create(
      cq, shutdown_manager, mock, kTestDeadline, std::chrono::seconds(600));

  auto done = shutdown_manager->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});

  batch_callback(GenerateMessages("0-", 3));
  ASSERT_EQ(1U, fake_cq->size());

  // Ack one of the messages and then fire the timer. The expectations set above
  // will verify that only the remaining messages have their lease extended.
  uut->AckMessage("ack-0-1");
  fake_cq->SimulateCompletion(true);
  ASSERT_EQ(1U, fake_cq->size());

  // Ack one more message and trigger the new timer.
  uut->NackMessage("ack-0-2");
  fake_cq->SimulateCompletion(true);
  ASSERT_EQ(1U, fake_cq->size());

  shutdown_manager->MarkAsShutdown(__func__, Status{});
  uut->Shutdown();

  fake_cq->SimulateCompletion(false);
  ASSERT_EQ(0U, fake_cq->size());
  EXPECT_THAT(done.get(), IsOk());
}

TEST(SubscriptionLeaseManagementTest, ShutdownOnError) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(fake_cq);

  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::Create(cq, shutdown_manager, mock,
                                                 std::chrono::seconds(345),
                                                 std::chrono::seconds(600));

  auto done = shutdown_manager->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});
  batch_callback(GenerateMessages("0-", 3));
  EXPECT_EQ(1U, fake_cq->size());

  batch_callback(StatusOr<google::pubsub::v1::StreamingPullResponse>(
      Status(StatusCode::kPermissionDenied, "uh-oh")));
  ASSERT_EQ(1U, fake_cq->size());

  fake_cq->SimulateCompletion(false);
  ASSERT_EQ(0U, fake_cq->size());
  EXPECT_THAT(done.get(), StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify ExtendLeases does not call the `child` object.
TEST(SubscriptionLeaseManagementTest, DoesNotPropagateExtendLeases) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::Create(
      background.cq(), shutdown_manager, mock, std::chrono::seconds(30),
      std::chrono::seconds(600));

  uut->ExtendLeases({"a", "b", "c"}, std::chrono::seconds(10));
}

TEST(SubscriptionLeaseManagementTest, UsesDeadlineExtension) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  BatchCallback batch_callback;
  EXPECT_CALL(*mock, Start).WillOnce([&](BatchCallback cb) {
    batch_callback = std::move(cb);
  });

  auto constexpr kTestDeadline = std::chrono::seconds(345);
  auto constexpr kTestExtension = std::chrono::seconds(100);
  {
    ::testing::InSequence sequence;
    // No messages acked.
    EXPECT_CALL(*mock, ExtendLeases)
        // Then a simulated timer refreshes the lease for the message.
        .WillOnce([&](std::vector<std::string> const& ack_ids,
                      std::chrono::seconds extension) {
          EXPECT_THAT(ack_ids, UnorderedElementsAre("ack-0-0"));
          EXPECT_LE(std::abs((kTestExtension - extension).count()), 2);
          return make_ready_future(Status{});
        });
    // Then the unhandled message is nacked on shutdown.
    EXPECT_CALL(*mock, BulkNack(UnorderedElementsAre("ack-0-0")))
        .WillOnce([](std::vector<std::string> const&) {
          return make_ready_future(Status{});
        });
    EXPECT_CALL(*mock, Shutdown).Times(1);
  }

  auto fake_cq = std::make_shared<FakeCompletionQueueImpl>();
  CompletionQueue cq(fake_cq);

  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionLeaseManagement::Create(cq, shutdown_manager, mock,
                                                 kTestDeadline, kTestExtension);

  auto done = shutdown_manager->Start({});
  uut->Start([](StatusOr<google::pubsub::v1::StreamingPullResponse> const&) {});

  batch_callback(GenerateMessages("0-", 1));
  ASSERT_EQ(1U, fake_cq->size());

  // Ignore message and then fire the timer. This will extend the deadline.
  fake_cq->SimulateCompletion(true);
  ASSERT_EQ(1U, fake_cq->size());

  shutdown_manager->MarkAsShutdown(__func__, Status{});
  uut->Shutdown();
  fake_cq->SimulateCompletion(false);
  ASSERT_EQ(0U, fake_cq->size());
  EXPECT_THAT(done.get(), IsOk());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
