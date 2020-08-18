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

#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/testing/mock_subscriber_stub.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/mock_completion_queue.h"
#include <gmock/gmock.h>
#include <atomic>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::AtLeast;

/// @test Verify session can schedule parallel callbacks.
TEST(SubscriptionConcurrentControlTest, ScheduleParallelCallbacks) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  std::mutex mu;
  int count = 0;
  auto constexpr kBatchSize = 8;
  auto constexpr kTestHwm = 4;
  static_assert(kBatchSize % kTestHwm == 0,
                "batch size must be a multiple of hwm for this test");

  auto generate = [&](google::cloud::CompletionQueue&,
                      std::unique_ptr<grpc::ClientContext>,
                      google::pubsub::v1::PullRequest const& request) {
    EXPECT_EQ(subscription.FullName(), request.subscription());
    google::pubsub::v1::PullResponse response;
    for (int i = 0; i != kBatchSize; ++i) {
      auto& m = *response.add_received_messages();
      std::lock_guard<std::mutex> lk(mu);
      m.set_ack_id("test-ack-id-" + std::to_string(count));
      m.mutable_message()->set_message_id("test-message-id-" +
                                          std::to_string(count));
      ++count;
    }
    return make_ready_future(make_status_or(response));
  };

  EXPECT_CALL(*mock, AsyncPull).Times(AtLeast(1)).WillRepeatedly(generate);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .Times(AtLeast(kBatchSize))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  google::cloud::CompletionQueue cq;
  std::vector<std::thread> tasks;
  std::generate_n(std::back_inserter(tasks), 4,
                  [&] { return std::thread([&cq] { cq.Run(); }); });

  std::mutex handlers_mu;
  std::condition_variable handlers_cv;
  std::deque<pubsub::AckHandler> handlers;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    std::lock_guard<std::mutex> lk(handlers_mu);
    handlers.push_back(std::move(h));
    if (handlers.size() >= kTestHwm) handlers_cv.notify_one();
  };

  auto session = SubscriptionSession::Create(
      mock, cq,
      {subscription.FullName(), handler,
       pubsub::SubscriptionOptions{}.set_concurrency_watermarks(0, kTestHwm)});
  auto response = session->Start();

  auto ack_current = [&](std::unique_lock<std::mutex> lk) {
    std::deque<pubsub::AckHandler> tmp;
    tmp.swap(handlers);
    lk.unlock();
    for (auto& h : tmp) std::move(h).ack();
  };
  for (auto i = 0; i != kBatchSize / kTestHwm; ++i) {
    std::unique_lock<std::mutex> lk(handlers_mu);
    handlers_cv.wait(lk, [&] { return handlers.size() >= kTestHwm; });
    ack_current(std::move(lk));
  }

  response.cancel();
  ack_current(std::unique_lock<std::mutex>(handlers_mu));
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  for (auto& t : tasks) t.join();
}

/// @test Verify session does not schedule too many parallel callbacks.
TEST(SubscriptionConcurrentControlTest, ScheduleParallelCallbacksLimits) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  auto constexpr kThreadCount = 4;
  auto constexpr kTestHwm = 16;
  auto constexpr kBatchSize = 32;
  auto constexpr kTotalMessages = 4 * kBatchSize;

  auto generate = [&](google::cloud::CompletionQueue&,
                      std::unique_ptr<grpc::ClientContext>,
                      google::pubsub::v1::PullRequest const&) {
    static std::atomic<int> call{0};
    auto const base = ++call;
    google::pubsub::v1::PullResponse response;
    for (int i = 0; i != kBatchSize; ++i) {
      auto const suffix = std::to_string(base) + "-" + std::to_string(i);
      auto& m = *response.add_received_messages();
      m.set_ack_id("test-ack-id-" + suffix);
      m.mutable_message()->set_message_id("test-message-id-" + suffix);
    }
    return make_ready_future(make_status_or(response));
  };

  EXPECT_CALL(*mock, AsyncPull).Times(AtLeast(1)).WillRepeatedly(generate);
  EXPECT_CALL(*mock, AsyncAcknowledge)
      .Times(AtLeast(kBatchSize))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline)
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  google::cloud::CompletionQueue cq;
  std::vector<std::thread> tasks;
  std::generate_n(std::back_inserter(tasks), kThreadCount,
                  [&] { return std::thread([&cq] { cq.Run(); }); });

  auto const callback_running_time = std::chrono::microseconds(100);
  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;
  std::mutex handlers_mu;
  std::condition_variable handlers_cv;
  int current_callback_count = 0;
  int maximum_callback_count = 0;
  int total_callbacks = 0;

  auto handler_done = [&](pubsub::AckHandler h) {
    std::unique_lock<std::mutex> lk(handlers_mu);
    --current_callback_count;
    std::move(h).ack();
    if (++total_callbacks < kTotalMessages) return;
    lk.unlock();
    handlers_cv.notify_one();
  };

  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    std::unique_lock<std::mutex> lk(handlers_mu);
    ++current_callback_count;
    maximum_callback_count =
        (std::max)(current_callback_count, maximum_callback_count);
    lk.unlock();
    struct MoveCapture {
      pubsub::AckHandler h;
      std::function<void(pubsub::AckHandler)> done;
      void operator()(TimerFuture) { done(std::move(h)); }
    };
    cq.MakeRelativeTimer(callback_running_time)
        .then(MoveCapture{std::move(h), handler_done});
  };

  auto session = [&] {
    return SubscriptionSession::Create(
               mock, cq,
               {subscription.FullName(), handler,
                pubsub::SubscriptionOptions{}.set_concurrency_watermarks(
                    kTestHwm / 2, kTestHwm)})
        ->Start();
  }();

  {
    std::unique_lock<std::mutex> lk(handlers_mu);
    handlers_cv.wait(lk, [&] { return total_callbacks >= kTotalMessages; });
  }
  session.cancel();
  auto status = session.get();
  EXPECT_STATUS_OK(status);

  EXPECT_LT(0, maximum_callback_count);
  EXPECT_GE(kTestHwm, maximum_callback_count);

  cq.Shutdown();
  for (auto& t : tasks) t.join();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
