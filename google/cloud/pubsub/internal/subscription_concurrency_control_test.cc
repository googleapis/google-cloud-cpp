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

#include "google/cloud/pubsub/internal/subscription_concurrency_control.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/testing/mock_subscription_message_source.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::AtLeast;
using ::testing::StartsWith;

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

  void PushMessages(MessageCallback const& cb, std::size_t n) {
    std::unique_lock<std::mutex> lk(messages_mu_);
    for (std::size_t i = 0; i != n && !messages_.empty(); ++i) {
      auto m = std::move(messages_.front());
      messages_.pop_front();
      lk.unlock();
      cb(std::move(m));
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
  MessageCallback message_callback;
  auto push_messages = [&](std::size_t n) {
    PushMessages(message_callback, n);
  };
  PrepareMessages("ack-0-", 2);
  PrepareMessages("ack-1-", 3);
  EXPECT_CALL(*source, Shutdown).Times(1);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&message_callback](MessageCallback cb) {
          message_callback = std::move(cb);
        });
    EXPECT_CALL(*source, Read(1))
        .Times(AtLeast(5))
        .WillRepeatedly(push_messages);
  }
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, AckMessage("ack-0-0"));
    EXPECT_CALL(*source, NackMessage("ack-0-1"));
    EXPECT_CALL(*source, AckMessage("ack-1-0"));
    EXPECT_CALL(*source, NackMessage("ack-1-1"));
    EXPECT_CALL(*source, NackMessage("ack-1-2"));
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;

  // Create the unit under test, configured to run 1 event at a time, this makes
  // it easier to setup expectations.
  auto shutdown = std::make_shared<SessionShutdownManager>();

  auto uut = SubscriptionConcurrencyControl::Create(
      background.cq(), shutdown, source, /*max_concurrency=*/1);

  std::mutex handler_mu;
  std::condition_variable handler_cv;
  std::deque<pubsub::AckHandler> ack_handlers;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    std::lock_guard<std::mutex> lk(handler_mu);
    ack_handlers.push_back(std::move(h));
    handler_cv.notify_one();
  };
  auto pull_next = [&] {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return !ack_handlers.empty(); });
    auto h = std::move(ack_handlers.front());
    ack_handlers.pop_front();
    return h;
  };

  auto done = shutdown->Start({});
  uut->Start(handler);

  auto h = pull_next();
  std::move(h).ack();
  h = pull_next();
  std::move(h).nack();

  h = pull_next();
  std::move(h).ack();
  h = pull_next();
  std::move(h).nack();
  h = pull_next();
  std::move(h).nack();

  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  EXPECT_THAT(done.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl schedules multiple callbacks.
TEST_F(SubscriptionConcurrencyControlTest, ParallelCallbacks) {
  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();
  MessageCallback message_callback;
  EXPECT_CALL(*source, Shutdown).Times(1);
  PrepareMessages("ack-0-", 8);
  PrepareMessages("ack-1-", 8);
  auto push_messages = [&](std::size_t n) {
    PushMessages(message_callback, n);
  };
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&message_callback](MessageCallback cb) {
          message_callback = std::move(cb);
        });
    EXPECT_CALL(*source, Read(4)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read(1)).WillOnce(push_messages);
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, AckMessage(StartsWith("ack-0-"))).Times(8);
    EXPECT_CALL(*source, AckMessage(StartsWith("ack-1-"))).Times(1);
    EXPECT_CALL(*source, NackMessage(StartsWith("ack-1-"))).Times(1);
    EXPECT_CALL(*source, AckMessage(StartsWith("ack-1-"))).Times(1);
    EXPECT_CALL(*source, NackMessage(StartsWith("ack-1-"))).Times(5);
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);
  auto shutdown = std::make_shared<SessionShutdownManager>();
  // Create the unit under test, configured to run at most 4 events at a time.
  auto uut =
      SubscriptionConcurrencyControl::Create(background.cq(), shutdown, source,
                                             /*max_concurrency=*/4);

  std::mutex handler_mu;
  std::condition_variable handler_cv;
  std::deque<pubsub::AckHandler> ack_handlers;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    std::lock_guard<std::mutex> lk(handler_mu);
    ack_handlers.push_back(std::move(h));
    handler_cv.notify_one();
  };
  auto wait_n = [&](std::size_t n) {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return ack_handlers.size() >= n; });
  };
  auto pull_next = [&] {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return !ack_handlers.empty(); });
    auto h = std::move(ack_handlers.front());
    ack_handlers.pop_front();
    return h;
  };

  auto done = shutdown->Start({});
  uut->Start(handler);

  wait_n(4);
  for (int i = 0; i != 2; ++i) pull_next().ack();
  wait_n(4);
  for (int i = 0; i != 2; ++i) pull_next().ack();
  wait_n(4);
  for (int i = 0; i != 4; ++i) pull_next().ack();

  pull_next().ack();
  pull_next().nack();
  pull_next().ack();
  for (int i = 0; i != 5; ++i) pull_next().nack();

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
  MessageCallback message_callback;
  PrepareMessages("ack-0-", kCallbackCount);
  PrepareMessages("ack-1-", 8);
  auto push_messages = [&](std::size_t n) {
    PushMessages(message_callback, n);
  };
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&message_callback](MessageCallback cb) {
          message_callback = std::move(cb);
        });
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }

  EXPECT_CALL(*source, Shutdown).Times(1);
  EXPECT_CALL(*source, AckMessage).Times(AtLeast(kCallbackCount));
  EXPECT_CALL(*source, NackMessage).Times(AtLeast(0));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(
      2 * kMaxConcurrency);

  // Create the unit under test, configured to run 1 event at a time, this makes
  // it easier to setup expectations.
  auto shutdown = std::make_shared<SessionShutdownManager>();

  auto uut = SubscriptionConcurrencyControl::Create(background.cq(), shutdown,
                                                    source, kMaxConcurrency);

  std::mutex handler_mu;
  std::condition_variable handler_cv;

  int current_callbacks = 0;
  int total_callbacks = 0;
  int observed_hwm = 0;
  auto delayed_handler = [&](pubsub::AckHandler h) {
    {
      std::lock_guard<std::mutex> lk(handler_mu);
      --current_callbacks;
      if (++total_callbacks > kCallbackCount) return;
    }
    handler_cv.notify_one();
    std::move(h).ack();
  };
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    {
      std::lock_guard<std::mutex> lk(handler_mu);
      ++current_callbacks;
      observed_hwm = (std::max)(observed_hwm, current_callbacks);
    }
    struct DelayedHandler {
      pubsub::AckHandler h;
      std::function<void(pubsub::AckHandler)> handler;
      void operator()(future<StatusOr<std::chrono::system_clock::time_point>>) {
        handler(std::move(h));
      }
    };
    background.cq()
        .MakeRelativeTimer(std::chrono::microseconds(100))
        .then(DelayedHandler{std::move(h), delayed_handler});
  };

  auto done = shutdown->Start({});
  uut->Start(handler);

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
  MessageCallback message_callback;
  PrepareMessages("ack-0-", kTestDoneThreshold + 1);
  PrepareMessages("ack-1-", kTestDoneThreshold);
  auto push_messages = [&](std::size_t n) {
    PushMessages(message_callback, n);
  };
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&message_callback](MessageCallback cb) {
          message_callback = std::move(cb);
        });
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }

  EXPECT_CALL(*source, Shutdown).Times(1);
  EXPECT_CALL(*source, AckMessage).Times(AtLeast(1));
  EXPECT_CALL(*source, NackMessage).Times(AtLeast(1));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);

  std::mutex handler_mu;
  std::condition_variable handler_cv;
  int message_counter = 0;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    std::unique_lock<std::mutex> lk(handler_mu);
    if (++message_counter >= kTestDoneThreshold) {
      handler_cv.notify_one();
      return;
    }
    if (message_counter >= kNackThreshold) return;
    std::move(h).ack();
  };

  // Transfer ownership to a future, like we would do for a fully configured
  auto session = [&] {
    auto shutdown = std::make_shared<SessionShutdownManager>();

    auto uut = SubscriptionConcurrencyControl::Create(
        background.cq(), shutdown, source, /*max_concurrency=*/4);
    promise<Status> p([shutdown, uut] {
      shutdown->MarkAsShutdown("test-function-", {});
      uut->Shutdown();
    });

    auto f = shutdown->Start(std::move(p));
    uut->Start(std::move(handler));
    return f;
  }();

  {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return message_counter >= kTestDoneThreshold; });
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
  MessageCallback message_callback;
  PrepareMessages("ack-0-", kTestDoneThreshold + 1);
  PrepareMessages("ack-1-", kTestDoneThreshold);
  auto push_messages = [&](std::size_t n) {
    PushMessages(message_callback, n);
  };
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&message_callback](MessageCallback cb) {
          message_callback = std::move(cb);
        });
    EXPECT_CALL(*source, Read).WillRepeatedly(push_messages);
  }

  EXPECT_CALL(*source, Shutdown).Times(1);
  EXPECT_CALL(*source, AckMessage).Times(AtLeast(1));

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background(4);

  std::mutex handler_mu;
  std::condition_variable handler_cv;
  int message_counter = 0;
  auto handler = [&](pubsub::Message const&, pubsub::AckHandler h) {
    std::move(h).ack();
    // Sleep after the `ack()` call to more easily reproduce #5148
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    std::unique_lock<std::mutex> lk(handler_mu);
    if (++message_counter >= kTestDoneThreshold) handler_cv.notify_one();
  };

  // Transfer ownership to a future. The library also does this for a fully
  // configured session in `Subscriber::Subscribe()`.
  auto session = [&] {
    auto shutdown = std::make_shared<SessionShutdownManager>();

    auto uut = SubscriptionConcurrencyControl::Create(
        background.cq(), shutdown, source, /*max_concurrency=*/4);
    promise<Status> p([shutdown, uut] {
      shutdown->MarkAsShutdown("test-function-", {});
      uut->Shutdown();
    });

    auto f = shutdown->Start(std::move(p));
    uut->Start(std::move(handler));
    return f;
  }();

  {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return message_counter >= kTestDoneThreshold; });
  }
  session.cancel();
  EXPECT_THAT(session.get(), IsOk());
}

/// @test Verify SubscriptionConcurrencyControl preserves message contents.
TEST_F(SubscriptionConcurrencyControlTest, MessageContents) {
  auto source =
      std::make_shared<pubsub_testing::MockSubscriptionMessageSource>();
  MessageCallback message_callback;
  auto push_messages = [&](std::size_t n) {
    PushMessages(message_callback, n);
  };
  PrepareMessages("ack-0-", 3);
  PrepareMessages("ack-1-", 2);
  EXPECT_CALL(*source, Shutdown).Times(1);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*source, Start)
        .WillOnce([&message_callback](MessageCallback cb) {
          message_callback = std::move(cb);
        });
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
      background.cq(), shutdown, source, /*max_concurrency=*/10);

  std::mutex handler_mu;
  std::condition_variable handler_cv;
  std::vector<std::pair<pubsub::Message, pubsub::AckHandler>> messages;
  auto handler = [&](pubsub::Message const& m, pubsub::AckHandler h) {
    std::lock_guard<std::mutex> lk(handler_mu);
    messages.emplace_back(std::move(m), std::move(h));
    handler_cv.notify_one();
  };
  auto wait_message_count = [&](std::size_t n) {
    std::unique_lock<std::mutex> lk(handler_mu);
    handler_cv.wait(lk, [&] { return messages.size() >= n; });
  };

  auto done = shutdown->Start({});
  uut->Start(handler);
  wait_message_count(5);

  // We only push 5 messages so after this no more messages will show up.
  // Grab the mutex to avoid false positives in TSAN.
  std::unique_lock<std::mutex> lk(handler_mu);
  for (auto& p : messages) {
    EXPECT_THAT(p.first.message_id(), StartsWith("message:"));
    auto const suffix = p.first.message_id().substr(sizeof("message:") - 1);
    EXPECT_EQ(42, p.second.delivery_attempt());
    EXPECT_EQ(p.first.data(), "data:" + suffix);
    EXPECT_EQ(p.first.attributes()["k0"], "l0:" + suffix);
    std::move(p.second).ack();
  }

  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  EXPECT_THAT(done.get(), IsOk());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
