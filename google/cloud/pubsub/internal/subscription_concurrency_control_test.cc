// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/subscription_concurrency_control.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/testing/mock_batch_callback.h"
#include "google/cloud/pubsub/testing/mock_subscription_message_source.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsub::ExactlyOnceAckHandler;
using ::google::cloud::testing_util::IsOk;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::StartsWith;

using Handler = std::unique_ptr<ExactlyOnceAckHandler::Impl>;

class SubscriptionConcurrencyControlTest : public ::testing::Test {
 protected:
  void PrepareMessages(std::string const& prefix, int n) {
    std::unique_lock<std::mutex> lk(messages_mu_);
    for (int i = 0; i != n; ++i) {
      google::pubsub::v1::ReceivedMessage m;
      auto const id = prefix + std::to_string(i);
      m.set_ack_id(id);
      m.set_delivery_attempt(42);
      m.mutable_message()->set_message_id("message:" + id);
      m.mutable_message()->set_data("data:" + id);
      (*m.mutable_message()->mutable_attributes())["k0"] = "l0:" + id;
      messages_.push_back(std::move(m));
    }
  }

  void PushMessages(std::shared_ptr<BatchCallback> const& cb, std::size_t n) {
    std::unique_lock<std::mutex> lk(messages_mu_);
    for (std::size_t i = 0; i != n && !messages_.empty(); ++i) {
      auto m = std::move(messages_.front());
      messages_.pop_front();
      lk.unlock();
      cb->message_callback(BatchCallback::ReceivedMessage{std::move(m)});
      lk.lock();
    }
  }

 private:
  std::mutex messages_mu_;
  std::deque<google::pubsub::v1::ReceivedMessage> messages_;
};

/// @test Verify SubscriptionConcurrencyControl works in the simple case.
TEST_F(SubscriptionConcurrencyControlTest, MessageLifecycle) {
  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();

  PrepareMessages("ack-0-", 2);
  PrepareMessages("ack-1-", 3);
  std::shared_ptr<BatchCallback> batch_callback;
  EXPECT_CALL(*source, Shutdown).Times(1);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&batch_callback](std::shared_ptr<BatchCallback> cb) {
          batch_callback = std::move(cb);
        });
    auto push_messages = [&](std::size_t n) {
      PushMessages(batch_callback, n);
    };
    EXPECT_CALL(*source, Read(1))
        .Times(AtLeast(5))
        .WillRepeatedly(push_messages);
  }
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, AckMessage("ack-0-0"))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    EXPECT_CALL(*source, NackMessage("ack-0-1"))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    EXPECT_CALL(*source, AckMessage("ack-1-0"))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    EXPECT_CALL(*source, NackMessage("ack-1-1"))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
    EXPECT_CALL(*source, NackMessage("ack-1-2"))
        .WillOnce(Return(ByMove(make_ready_future(Status{}))));
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;

  // Create the unit under test, configured to run 1 event at a time, this makes
  // it easier to setup expectations.
  auto shutdown = std::make_shared<SessionShutdownManager>();

  auto uut = SubscriptionConcurrencyControl::Create(
      background.cq(), shutdown, source,
      pubsub::Subscription("test-project", "test-sub"), /*max_concurrency=*/1);

  std::mutex queue_mu;
  std::condition_variable queue_cv;
  std::deque<Handler> ack_handlers;
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartConcurrencyControl).Times(5);
  EXPECT_CALL(*mock_batch_callback, EndConcurrencyControl).Times(5);
  EXPECT_CALL(*mock_batch_callback, user_callback)
      .WillRepeatedly([&](MessageCallback::MessageAndHandler m) {
        std::lock_guard<std::mutex> lk(queue_mu);
        ack_handlers.push_back(std::move(m.ack_handler));
        queue_cv.notify_one();
      });

  auto pull_next = [&] {
    std::unique_lock<std::mutex> lk(queue_mu);
    queue_cv.wait(lk, [&] { return !ack_handlers.empty(); });
    auto h = std::move(ack_handlers.front());
    ack_handlers.pop_front();
    return h;
  };

  auto done = shutdown->Start({});
  uut->Start(mock_batch_callback);

  auto h = pull_next();
  h->ack();
  h = pull_next();
  h->nack();

  h = pull_next();
  h->ack();
  h = pull_next();
  h->nack();
  h = pull_next();
  h->nack();

  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  EXPECT_THAT(done.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl schedules multiple callbacks.
TEST_F(SubscriptionConcurrencyControlTest, ParallelCallbacks) {
  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();
  EXPECT_CALL(*source, Shutdown).Times(1);
  PrepareMessages("ack-0-", 8);
  PrepareMessages("ack-1-", 8);

  std::shared_ptr<BatchCallback> batch_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&batch_callback](std::shared_ptr<BatchCallback> cb) {
          batch_callback = std::move(cb);
        });
    auto push_messages = [&](std::size_t n) {
      PushMessages(batch_callback, n);
    };
    EXPECT_CALL(*source, Read(4)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, AckMessage(StartsWith("ack-0-")))
        .Times(8)
        .WillRepeatedly([] { return make_ready_future(Status{}); });
    EXPECT_CALL(*source, AckMessage(StartsWith("ack-1-")))
        .Times(1)
        .WillRepeatedly([] { return make_ready_future(Status{}); });
    EXPECT_CALL(*source, NackMessage(StartsWith("ack-1-")))
        .Times(1)
        .WillRepeatedly([] { return make_ready_future(Status{}); });
    EXPECT_CALL(*source, AckMessage(StartsWith("ack-1-")))
        .Times(1)
        .WillRepeatedly([] { return make_ready_future(Status{}); });
    EXPECT_CALL(*source, NackMessage(StartsWith("ack-1-")))
        .Times(5)
        .WillRepeatedly([] { return make_ready_future(Status{}); });
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto shutdown = std::make_shared<SessionShutdownManager>();
  // Create the unit under test, configured to run at most 4 events at a time.
  auto uut = SubscriptionConcurrencyControl::Create(
      background.cq(), shutdown, source,
      pubsub::Subscription("test-project", "test-sub"),
      /*max_concurrency=*/4);

  std::mutex callback_mu;
  std::condition_variable callback_cv;
  std::deque<Handler> ack_handlers;
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartConcurrencyControl).Times(16);
  EXPECT_CALL(*mock_batch_callback, EndConcurrencyControl).Times(16);
  EXPECT_CALL(*mock_batch_callback, user_callback)
      .WillRepeatedly([&](MessageCallback::MessageAndHandler m) {
        std::lock_guard<std::mutex> lk(callback_mu);
        ack_handlers.push_back(std::move(m.ack_handler));
        callback_cv.notify_one();
      });
  auto wait_n = [&](std::size_t n) {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return ack_handlers.size() >= n; });
  };
  auto pull_next = [&] {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return !ack_handlers.empty(); });
    auto h = std::move(ack_handlers.front());
    ack_handlers.pop_front();
    return h;
  };

  auto done = shutdown->Start({});
  uut->Start(mock_batch_callback);

  wait_n(4);
  for (int i = 0; i != 2; ++i) pull_next()->ack();
  wait_n(4);
  for (int i = 0; i != 2; ++i) pull_next()->ack();
  wait_n(4);
  for (int i = 0; i != 4; ++i) pull_next()->ack();

  pull_next()->ack();
  pull_next()->nack();
  pull_next()->ack();
  for (int i = 0; i != 5; ++i) pull_next()->nack();

  shutdown->MarkAsShutdown(__func__, Status{});
  uut->Shutdown();
  EXPECT_THAT(done.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl respects the concurrency limit.
TEST_F(SubscriptionConcurrencyControlTest,
       ParallelCallbacksRespectConcurrencyLimit) {
  auto constexpr kMaxConcurrency = 8;
  auto constexpr kCallbackCount = 200;

  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();
  PrepareMessages("ack-0-", kCallbackCount);
  PrepareMessages("ack-1-", 8);

  std::shared_ptr<BatchCallback> batch_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&batch_callback](std::shared_ptr<BatchCallback> cb) {
          batch_callback = std::move(cb);
        });
    auto push_messages = [&](std::size_t n) {
      PushMessages(batch_callback, n);
    };
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }

  EXPECT_CALL(*source, Shutdown).Times(1);
  EXPECT_CALL(*source, AckMessage)
      .Times(AtLeast(kCallbackCount))
      .WillRepeatedly([] { return make_ready_future(Status{}); });
  EXPECT_CALL(*source, NackMessage).WillRepeatedly([] {
    return make_ready_future(Status{});
  });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(
      2 * kMaxConcurrency);

  // Create the unit under test, configured to run 1 event at a time, this makes
  // it easier to setup expectations.
  auto shutdown = std::make_shared<SessionShutdownManager>();

  auto uut = SubscriptionConcurrencyControl::Create(
      background.cq(), shutdown, source,
      pubsub::Subscription("test-project", "test-sub"), kMaxConcurrency);

  std::mutex handler_mu;
  std::condition_variable handler_cv;

  int current_callbacks = 0;
  int total_callbacks = 0;
  int observed_hwm = 0;
  auto delayed_callback = [&](Handler h) {
    pubsub_internal::ExactlyOnceAckHandler wrapper(std::move(h));
    {
      std::lock_guard<std::mutex> lk(handler_mu);
      --current_callbacks;
      if (++total_callbacks > kCallbackCount) return;
    }
    handler_cv.notify_one();
    std::move(wrapper).ack();
  };
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartConcurrencyControl)
      .Times(AtLeast(kCallbackCount));
  EXPECT_CALL(*mock_batch_callback, EndConcurrencyControl)
      .Times(AtLeast(kCallbackCount));
  EXPECT_CALL(*mock_batch_callback, user_callback)
      .WillRepeatedly([&](MessageCallback::MessageAndHandler m) {
        {
          std::lock_guard<std::mutex> lk(handler_mu);
          ++current_callbacks;
          observed_hwm = (std::max)(observed_hwm, current_callbacks);
        }
        background.cq()
            .MakeRelativeTimer(std::chrono::microseconds(100))
            .then([h = std::move(m.ack_handler), callback = delayed_callback](
                      auto) mutable { callback(std::move(h)); });
      });

  auto done = shutdown->Start({});
  uut->Start(mock_batch_callback);

  {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return total_callbacks >= kCallbackCount; });
    EXPECT_GE(kMaxConcurrency, observed_hwm);
  }

  shutdown->MarkAsShutdown(__func__, Status{});
  uut->Shutdown();

  EXPECT_THAT(done.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl shutdown.
TEST_F(SubscriptionConcurrencyControlTest, CleanShutdown) {
  auto constexpr kNackThreshold = 10;
  auto constexpr kTestDoneThreshold = 2 * kNackThreshold;

  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();
  PrepareMessages("ack-0-", kTestDoneThreshold + 1);
  PrepareMessages("ack-1-", kTestDoneThreshold);
  std::shared_ptr<BatchCallback> batch_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&batch_callback](std::shared_ptr<BatchCallback> cb) {
          batch_callback = std::move(cb);
        });
    auto push_messages = [&](std::size_t n) {
      PushMessages(batch_callback, n);
    };
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }

  EXPECT_CALL(*source, Shutdown).Times(1);
  EXPECT_CALL(*source, AckMessage).Times(AtLeast(1)).WillRepeatedly([] {
    return make_ready_future(Status{});
  });
  EXPECT_CALL(*source, NackMessage).Times(AtLeast(1)).WillRepeatedly([] {
    return make_ready_future(Status{});
  });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);

  std::mutex callback_mu;
  std::condition_variable callback_cv;
  int message_counter = 0;
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartConcurrencyControl)
      .Times(AtLeast(kTestDoneThreshold));
  EXPECT_CALL(*mock_batch_callback, EndConcurrencyControl)
      .Times(AtLeast(kTestDoneThreshold));
  EXPECT_CALL(*mock_batch_callback, user_callback)
      .WillRepeatedly([&](MessageCallback::MessageAndHandler m) {
        pubsub_internal::ExactlyOnceAckHandler wrapper(
            std::move(m.ack_handler));
        std::unique_lock<std::mutex> lk(callback_mu);
        if (++message_counter >= kTestDoneThreshold) {
          callback_cv.notify_one();
          return;
        }
        if (message_counter >= kNackThreshold) return;
        std::move(wrapper).ack();
      });

  // Transfer ownership to a future, like we would do for a fully configured
  auto session = [&] {
    auto shutdown = std::make_shared<SessionShutdownManager>();

    auto uut = SubscriptionConcurrencyControl::Create(
        background.cq(), shutdown, source,
        pubsub::Subscription("test-project", "test-sub"),
        /*max_concurrency=*/4);
    promise<Status> p([shutdown, uut] {
      shutdown->MarkAsShutdown("test-function-", {});
      uut->Shutdown();
    });

    auto f = shutdown->Start(std::move(p));
    uut->Start(std::move(mock_batch_callback));
    return f;
  }();

  {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return message_counter >= kTestDoneThreshold; });
  }
  session.cancel();
  EXPECT_THAT(session.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl shutdown with early acks.
TEST_F(SubscriptionConcurrencyControlTest, CleanShutdownEarlyAcks) {
  auto constexpr kNackThreshold = 16;
  auto constexpr kTestDoneThreshold = 2 * kNackThreshold;

  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();
  PrepareMessages("ack-0-", kTestDoneThreshold + 1);
  PrepareMessages("ack-1-", kTestDoneThreshold);
  std::shared_ptr<BatchCallback> batch_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&batch_callback](std::shared_ptr<BatchCallback> cb) {
          batch_callback = std::move(cb);
        });
    auto push_messages = [&](std::size_t n) {
      PushMessages(batch_callback, n);
    };
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }

  EXPECT_CALL(*source, Shutdown).Times(1);
  EXPECT_CALL(*source, AckMessage).Times(AtLeast(1)).WillRepeatedly([] {
    return make_ready_future(Status{});
  });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);

  std::mutex callback_mu;
  std::condition_variable callback_cv;
  int message_counter = 0;
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartConcurrencyControl)
      .Times(AtLeast(kTestDoneThreshold));
  EXPECT_CALL(*mock_batch_callback, EndConcurrencyControl)
      .Times(AtLeast(kTestDoneThreshold));
  EXPECT_CALL(*mock_batch_callback, user_callback)
      .WillRepeatedly([&](MessageCallback::MessageAndHandler m) {
        m.ack_handler->ack();
        // Sleep after the `ack()` call to more easily reproduce #5148
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        std::unique_lock<std::mutex> lk(callback_mu);
        if (++message_counter >= kTestDoneThreshold) callback_cv.notify_one();
      });

  // Transfer ownership to a future. The library also does this for a fully
  // configured session in `Subscriber::Subscribe()`.
  auto session = [&] {
    auto shutdown = std::make_shared<SessionShutdownManager>();

    auto uut = SubscriptionConcurrencyControl::Create(
        background.cq(), shutdown, source,
        pubsub::Subscription("test-project", "test-sub"),
        /*max_concurrency=*/4);
    promise<Status> p([shutdown, uut] {
      shutdown->MarkAsShutdown("test-function-", {});
      uut->Shutdown();
    });

    auto f = shutdown->Start(std::move(p));
    uut->Start(std::move(mock_batch_callback));
    return f;
  }();

  {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return message_counter >= kTestDoneThreshold; });
  }
  session.cancel();
  EXPECT_THAT(session.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl preserves message contents.
TEST_F(SubscriptionConcurrencyControlTest, MessageContents) {
  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();

  PrepareMessages("ack-0-", 3);
  PrepareMessages("ack-1-", 2);
  EXPECT_CALL(*source, Shutdown).Times(1);
  std::shared_ptr<BatchCallback> batch_callback;
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&batch_callback](std::shared_ptr<BatchCallback> cb) {
          batch_callback = std::move(cb);
        });
    auto push_messages = [&](std::size_t n) {
      PushMessages(batch_callback, n);
    };
    EXPECT_CALL(*source, Read(10)).WillOnce(push_messages);
  }

  EXPECT_CALL(*source, AckMessage)
      .Times(5)
      .WillRepeatedly(
          [](std::string const&) { return make_ready_future(Status{}); });
  EXPECT_CALL(*source, Read(1)).Times(5);

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);

  // Create the unit under test, configured to run 1 event at a time, this makes
  // it easier to setup expectations.
  auto shutdown = std::make_shared<SessionShutdownManager>();

  auto uut = SubscriptionConcurrencyControl::Create(
      background.cq(), shutdown, source,
      pubsub::Subscription("test-project", "test-sub"), /*max_concurrency=*/10);

  std::mutex callback_mu;
  std::condition_variable callback_cv;
  std::vector<std::pair<pubsub::Message, Handler>> messages;
  auto mock_batch_callback =
      std::make_shared<pubsub_testing::MockBatchCallback>();
  EXPECT_CALL(*mock_batch_callback, StartConcurrencyControl).Times(5);
  EXPECT_CALL(*mock_batch_callback, EndConcurrencyControl).Times(5);
  EXPECT_CALL(*mock_batch_callback, user_callback)
      .WillRepeatedly([&](MessageCallback::MessageAndHandler m) {
        std::lock_guard<std::mutex> lk(callback_mu);
        messages.emplace_back(std::move(m.message), std::move(m.ack_handler));
        callback_cv.notify_one();
      });
  auto wait_message_count = [&](std::size_t n) {
    std::unique_lock<std::mutex> lk(callback_mu);
    callback_cv.wait(lk, [&] { return messages.size() >= n; });
  };

  auto done = shutdown->Start({});
  uut->Start(mock_batch_callback);
  wait_message_count(5);

  // We only push 5 messages so after this no more messages will show up.
  // Grab the mutex to avoid false positives in TSAN.
  std::unique_lock<std::mutex> lk(callback_mu);
  for (auto& p : messages) {
    EXPECT_THAT(p.first.message_id(), StartsWith("message:"));
    auto const suffix = p.first.message_id().substr(sizeof("message:") - 1);
    EXPECT_EQ(42, p.second->delivery_attempt());
    EXPECT_EQ(p.first.data(), "data:" + suffix);
    EXPECT_EQ(p.first.attributes()["k0"], "l0:" + suffix);
    std::move(p.second)->ack();
  }

  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  EXPECT_THAT(done.get(), IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
