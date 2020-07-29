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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/mock_completion_queue.h"
#include <gmock/gmock.h>
#include <atomic>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::StartsWith;

/// @test Verify callbacks are scheduled in the background threads.
TEST(SubscriptionSessionTest, ScheduleCallbacks) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  std::mutex mu;
  int count = 0;
  EXPECT_CALL(*mock, AsyncPull(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly([&](google::cloud::CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::pubsub::v1::PullRequest const& request) {
        EXPECT_EQ(subscription.FullName(), request.subscription());
        google::pubsub::v1::PullResponse response;
        for (int i = 0; i != 2; ++i) {
          auto& m = *response.add_received_messages();
          std::lock_guard<std::mutex> lk(mu);
          m.set_ack_id("test-ack-id-" + std::to_string(count));
          m.mutable_message()->set_message_id("test-message-id-" +
                                              std::to_string(count));
          ++count;
        }
        return make_ready_future(make_status_or(response));
      });

  std::atomic<int> expected_ack_id{0};
  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>,
              google::pubsub::v1::AcknowledgeRequest const& request) {
            EXPECT_EQ(subscription.FullName(), request.subscription());
            for (auto const& a : request.ack_ids()) {
              EXPECT_EQ("test-ack-id-" + std::to_string(expected_ack_id), a);
              ++expected_ack_id;
            }
            return make_ready_future(Status{});
          });
  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });

  google::cloud::CompletionQueue cq;
  std::vector<std::thread> tasks;
  std::generate_n(std::back_inserter(tasks), 4,
                  [&] { return std::thread([&cq] { cq.Run(); }); });
  std::set<std::thread::id> ids;
  auto const main_id = std::this_thread::get_id();
  std::transform(tasks.begin(), tasks.end(), std::inserter(ids, ids.end()),
                 [](std::thread const& t) { return t.get_id(); });

  std::atomic<int> expected_message_id{0};
  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    EXPECT_EQ("test-message-id-" + std::to_string(expected_message_id),
              m.message_id());
    auto pos = ids.find(std::this_thread::get_id());
    EXPECT_NE(ids.end(), pos);
    EXPECT_NE(main_id, std::this_thread::get_id());
    // Increment the counter before acking, as the ack() may trigger a new call
    // before this function gets to run.
    ++expected_message_id;
    std::move(h).ack();
  };

  auto session = SubscriptionSession::Create(
      mock, cq, {subscription.FullName(), handler, {}});
  auto response = session->Start();
  while (expected_ack_id.load() < 100) {
    auto s = response.wait_for(std::chrono::milliseconds(5));
    if (s != std::future_status::timeout) break;
  }
  response.cancel();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  for (auto& t : tasks) t.join();
}

/// @test Verify callbacks are scheduled in sequence.
TEST(SubscriptionSessionTest, SequencedCallbacks) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  std::mutex mu;
  int count = 0;
  auto generate_3 = [&](google::cloud::CompletionQueue&,
                        std::unique_ptr<grpc::ClientContext>,
                        google::pubsub::v1::PullRequest const& request) {
    EXPECT_EQ(subscription.FullName(), request.subscription());
    google::pubsub::v1::PullResponse response;
    for (int i = 0; i != 3; ++i) {
      auto& m = *response.add_received_messages();
      std::lock_guard<std::mutex> lk(mu);
      m.set_ack_id("test-ack-id-" + std::to_string(count));
      m.mutable_message()->set_message_id("test-message-id-" +
                                          std::to_string(count));
      ++count;
    }
    return make_ready_future(make_status_or(response));
  };

  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
      .WillRepeatedly([](google::cloud::CompletionQueue&,
                         std::unique_ptr<grpc::ClientContext>,
                         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      });
  {
    InSequence sequence;

    EXPECT_CALL(*mock, AsyncPull(_, _, _)).WillOnce(generate_3);
    EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
        .Times(3)
        .WillRepeatedly([](google::cloud::CompletionQueue&,
                           std::unique_ptr<grpc::ClientContext>,
                           google::pubsub::v1::AcknowledgeRequest const&) {
          return make_ready_future(Status{});
        });

    EXPECT_CALL(*mock, AsyncPull(_, _, _)).WillOnce(generate_3);
    EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
        .Times(3)
        .WillRepeatedly([](google::cloud::CompletionQueue&,
                           std::unique_ptr<grpc::ClientContext>,
                           google::pubsub::v1::AcknowledgeRequest const&) {
          return make_ready_future(Status{});
        });

    EXPECT_CALL(*mock, AsyncPull(_, _, _)).WillOnce(generate_3);
    EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
        .Times(3)
        .WillRepeatedly([](google::cloud::CompletionQueue&,
                           std::unique_ptr<grpc::ClientContext>,
                           google::pubsub::v1::AcknowledgeRequest const&) {
          return make_ready_future(Status{});
        });
  }

  promise<void> enough_messages;
  std::atomic<int> received_counter{0};
  auto constexpr kMaximumMessages = 9;
  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    auto c = received_counter.load();
    EXPECT_LE(c, kMaximumMessages);
    SCOPED_TRACE("Running for message " + m.message_id() +
                 ", counter=" + std::to_string(c));
    EXPECT_EQ("test-message-id-" + std::to_string(c), m.message_id());
    if (++received_counter == kMaximumMessages) {
      enough_messages.set_value();
    }
    std::move(h).ack();
  };

  google::cloud::CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });
  auto session = SubscriptionSession::Create(
      mock, cq, {subscription.FullName(), handler, {}});
  auto response = session->Start();
  enough_messages.get_future()
      .then([&](future<void>) { response.cancel(); })
      .get();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  t.join();
}

/// @test Verify callbacks are scheduled in sequence.
TEST(SubscriptionSessionTest, UpdateAckDeadlines) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriberStub>();
  pubsub::Subscription const subscription("test-project", "test-subscription");

  std::mutex mu;
  int count = 0;
  auto generate_3 = [&](google::cloud::CompletionQueue&,
                        std::unique_ptr<grpc::ClientContext>,
                        google::pubsub::v1::PullRequest const& request) {
    EXPECT_EQ(subscription.FullName(), request.subscription());
    google::pubsub::v1::PullResponse response;
    for (int i = 0; i != 3; ++i) {
      auto& m = *response.add_received_messages();
      std::lock_guard<std::mutex> lk(mu);
      m.set_ack_id("test-ack-id-" + std::to_string(count));
      m.mutable_message()->set_message_id("test-message-id-" +
                                          std::to_string(count));
      ++count;
    }
    return make_ready_future(make_status_or(response));
  };
  auto generate_ack_response =
      [](google::cloud::CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
         google::pubsub::v1::AcknowledgeRequest const&) {
        return make_ready_future(Status{});
      };
  auto ignore_modify_ack_deadline =
      [](google::cloud::CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
         google::pubsub::v1::ModifyAckDeadlineRequest const&) {
        return make_ready_future(Status{});
      };

  promise<void> deadline_0_updated;
  promise<void> deadline_1_updated;
  promise<void> deadline_2_updated;

  Expectation pull_0 =
      EXPECT_CALL(*mock, AsyncPull(_, _, _)).WillOnce(generate_3);

  // This may happen 0 or more times, depending on timing.
  Expectation modify_ack_0 = EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
                                 .WillRepeatedly(ignore_modify_ack_deadline);

  Expectation acks_0 = EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
                           .Times(2)
                           .WillRepeatedly(generate_ack_response);

  // After 2 acks the handler will block, which will (eventually) expire the
  // timer and create a update on the AckDeadline and trigger this
  // expectation. Here we release the handler to wait on the next event.
  Expectation modify_ack_1 =
      EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
          .After(modify_ack_0, acks_0)
          .WillOnce(
              [&](google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
                  google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
                EXPECT_EQ(1, request.ack_ids_size());
                if (request.ack_ids_size() == 1) {
                  EXPECT_EQ("test-ack-id-2", request.ack_ids(0));
                }
                // Allow callback to make progress.
                deadline_0_updated.set_value();
                return make_ready_future(Status{});
              })
          .WillRepeatedly(
              // Rarely we will get additional timers triggered, ignore them.
              ignore_modify_ack_deadline);

  Expectation acks_1 = EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
                           .After(modify_ack_1)
                           .WillOnce(generate_ack_response);

  EXPECT_CALL(*mock, AsyncPull(_, _, _)).After(acks_1).WillOnce(generate_3);
  Expectation acks_2 = EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
                           .After(acks_1)
                           .WillOnce(generate_ack_response)
                           .WillOnce(generate_ack_response);

  // Here the handler blocks for two deadline updates
  Expectation modify_ack_2 =
      EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
          .After(acks_2)
          .WillOnce(
              [&](google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
                  google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
                EXPECT_EQ(1, request.ack_ids_size());
                if (request.ack_ids_size() == 1) {
                  EXPECT_EQ("test-ack-id-5", request.ack_ids(0));
                }
                // Allow callback to make progress.
                deadline_1_updated.set_value();
                return make_ready_future(Status{});
              })
          .WillOnce(
              [&](google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
                  google::pubsub::v1::ModifyAckDeadlineRequest const& request) {
                EXPECT_EQ(1, request.ack_ids_size());
                if (request.ack_ids_size() == 1) {
                  EXPECT_EQ("test-ack-id-5", request.ack_ids(0));
                }
                // Allow callback to make progress.
                deadline_2_updated.set_value();
                return make_ready_future(Status{});
              })
          .WillRepeatedly(ignore_modify_ack_deadline);

  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
      .After(modify_ack_2)
      .WillOnce(generate_ack_response);

  Expectation pull_2 = EXPECT_CALL(*mock, AsyncPull(_, _, _))
                           .After(modify_ack_2)
                           .WillOnce(generate_3);

  EXPECT_CALL(*mock, AsyncAcknowledge(_, _, _))
      .After(pull_2)
      .WillOnce(generate_ack_response)
      .WillOnce(generate_ack_response)
      .WillOnce(generate_ack_response);
//  EXPECT_CALL(*mock, AsyncModifyAckDeadline(_, _, _))
//      .After(modify_ack_2)
//      .WillRepeatedly(ignore_modify_ack_deadline);

  promise<void> enough_messages;
  std::atomic<int> expected_message_id{0};
  auto constexpr kMaximumMessages = 9;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    auto const id = expected_message_id.load();
    if (id == 2) {
      deadline_0_updated.get_future().get();
    }
    if (id == 5) {
      deadline_1_updated.get_future().get();
      deadline_2_updated.get_future().get();
    }
    if (++expected_message_id == kMaximumMessages) {
      enough_messages.set_value();
    }
    std::move(h).ack();
  };

  google::cloud::CompletionQueue cq;
  std::vector<std::thread> pool;
  // we need more than one thread because `handler()` blocks and the CQ needs to
  // make progress.
  std::generate_n(std::back_inserter(pool), 4,
                  [&cq] { return std::thread{[&cq] { cq.Run(); }}; });

  auto session = SubscriptionSession::Create(
      mock, cq,
      {subscription.FullName(), handler,
       pubsub::SubscriptionOptions{}.set_max_deadline_time(
           std::chrono::seconds(60))});
  session->SetTestRefreshPeriod(std::chrono::milliseconds(50));
  auto response = session->Start();
  enough_messages.get_future()
      .then([&](future<void>) { response.cancel(); })
      .get();
  EXPECT_STATUS_OK(response.get());

  cq.Shutdown();
  for (auto& t : pool) t.join();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
