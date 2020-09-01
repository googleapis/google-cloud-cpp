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

#include "google/cloud/pubsub/internal/subscription_flow_control.h"
#include "google/cloud/pubsub/testing/mock_subscription_batch_source.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;

auto constexpr kUnlimitedCallbacks = 1000;
auto constexpr kDefaultSizeLwm = 10 * 1024 * 1024L;
auto constexpr kDefaultSizeHwm = 20 * 1024 * 1024L;

class SubscriptionFlowControlTest : public ::testing::Test {
 protected:
  static google::pubsub::v1::PullResponse GenerateMessages(
      std::string const& prefix, int count) {
    google::pubsub::v1::PullResponse response;
    for (int i = 0; i != count; ++i) {
      auto const id = prefix + std::to_string(i);
      auto& m = *response.add_received_messages();
      m.set_ack_id("ack-" + id);
      m.mutable_message()->set_message_id("message-" + id);
    }
    return response;
  }

  google::pubsub::v1::PullResponse GenerateSizedMessages(
      std::string const& prefix, int count, int size) {
    // see https://cloud.google.com/pubsub/pricing
    auto constexpr kMessageOverhead = 20;
    auto const payload = google::cloud::internal::Sample(
        generator_, size - kMessageOverhead, "0123456789");
    google::pubsub::v1::PullResponse response;
    for (int i = 0; i != count; ++i) {
      auto const id = prefix + std::to_string(i);
      auto& m = *response.add_received_messages();
      m.set_ack_id("ack-" + id);
      m.mutable_message()->set_message_id(payload);
    }
    return response;
  }

  future<void> AddAsyncPull() {
    std::unique_lock<std::mutex> lk(async_pulls_mu_);
    promise<void> p;
    auto f = p.get_future();
    async_pulls_.push_back(std::move(p));
    async_pulls_cv_.notify_one();
    return f;
  }

  void WaitAsyncPullCount(std::size_t n) {
    std::unique_lock<std::mutex> lk(async_pulls_mu_);
    async_pulls_cv_.wait(lk, [&] { return async_pulls_.size() >= n; });
  }

  std::vector<std::string> CurrentAcks() {
    std::unique_lock<std::mutex> lk(acks_mu_);
    auto tmp = std::move(acks_);
    acks_ = {};
    return tmp;
  }

  void SaveAcks(google::pubsub::v1::ReceivedMessage const& m) {
    std::unique_lock<std::mutex> lk(acks_mu_);
    acks_.push_back(m.ack_id());
    lk.unlock();
    acks_cv_.notify_one();
  }

  void WaitAcks() { WaitAcksCount(1); }

  void WaitAcksCount(std::size_t n) {
    std::unique_lock<std::mutex> lk(acks_mu_);
    acks_cv_.wait(lk, [&] { return acks_.size() >= n; });
  }

  std::mutex async_pulls_mu_;
  std::condition_variable async_pulls_cv_;
  std::vector<promise<void>> async_pulls_;

  std::mutex acks_mu_;
  std::condition_variable acks_cv_;
  std::vector<std::string> acks_;

  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
};

TEST_F(SubscriptionFlowControlTest, Basic) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);

  EXPECT_CALL(*mock, Pull)
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(1, max_messages);
        return AddAsyncPull().then([](future<void>) {
          return make_status_or(GenerateMessages("0-", 1));
        });
      })
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(1, max_messages);
        return AddAsyncPull().then([](future<void>) {
          return make_status_or(GenerateMessages("1-", 1));
        });
      })
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(1, max_messages);
        return AddAsyncPull().then([](future<void>) {
          return make_status_or(GenerateMessages("2-", 1));
        });
      });
  EXPECT_CALL(*mock, AckMessage)
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_THAT(ack_id, "ack-0-0");
        return make_ready_future(Status{});
      });
  EXPECT_CALL(*mock, NackMessage)
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_THAT(ack_id, "ack-1-0");
        return make_ready_future(Status{});
      });
  // There may be a few of these calls during shutdown. We have a separate test
  // for this, so we ignore them here.
  EXPECT_CALL(*mock, BulkNack)
      .WillRepeatedly([](std::vector<std::string> const&, std::size_t) {
        return make_ready_future(Status{});
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionFlowControl::Create(
      background.cq(), shutdown, mock, /*message_count_lwm=*/0,
      /*message_count_hwm=*/1,
      /*message_size_lwm=*/kDefaultSizeLwm,
      /*message_size_hwm=*/kDefaultSizeHwm);

  auto callback = [this](google::pubsub::v1::ReceivedMessage const& response) {
    SaveAcks(response);
  };

  auto done = shutdown->Start({});

  uut->Start(callback);
  uut->Read(kUnlimitedCallbacks);
  WaitAsyncPullCount(1);
  EXPECT_TRUE(CurrentAcks().empty());
  async_pulls_[0].set_value();
  WaitAcks();
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-0-0"));
  uut->AckMessage("ack-0-0", 0);

  WaitAsyncPullCount(2);
  async_pulls_[1].set_value();
  WaitAcks();
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-1-0"));
  uut->NackMessage("ack-1-0", 0);

  WaitAsyncPullCount(3);

  // Shut down things to prevent more calls, only then complete the last
  // asynchronous call.
  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  async_pulls_[2].set_value();

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

/// @test Verify that messages received after a shutdown are nacked.
TEST_F(SubscriptionFlowControlTest, NackOnShutdown) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, Pull).WillOnce([&](std::int32_t max_messages) {
      EXPECT_EQ(1, max_messages);
      return AddAsyncPull().then([](future<void>) {
        return make_status_or(GenerateMessages("0-", 3));
      });
    });
    EXPECT_CALL(*mock, BulkNack)
        .WillOnce([&](std::vector<std::string> const& ack_ids, std::size_t) {
          EXPECT_THAT(ack_ids, ElementsAre("ack-0-0", "ack-0-1", "ack-0-2"));
          return make_ready_future(Status{});
        });
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut =
      SubscriptionFlowControl::Create(background.cq(), shutdown, mock, 0, 1,
                                      /*message_size_lwm=*/kDefaultSizeLwm,
                                      /*message_size_hwm=*/kDefaultSizeHwm);

  auto done = shutdown->Start({});
  uut->Start([](google::pubsub::v1::ReceivedMessage const&) {});
  uut->Read(kUnlimitedCallbacks);
  WaitAsyncPullCount(1);

  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  async_pulls_[0].set_value();

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

/// @test Verify that messages received after a shutdown are nacked.
TEST_F(SubscriptionFlowControlTest, HandleOnPullError) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, Pull)
        .WillOnce([&](std::int32_t max_messages) {
          EXPECT_EQ(10, max_messages);
          return AddAsyncPull().then([](future<void>) {
            return make_status_or(GenerateMessages("0-", 3));
          });
        })
        .WillOnce([&](std::int32_t max_messages) {
          EXPECT_EQ(7, max_messages);
          return AddAsyncPull().then([](future<void>) {
            return StatusOr<google::pubsub::v1::PullResponse>(
                Status(StatusCode::kUnknown, "uh?"));
          });
        });
    EXPECT_CALL(*mock, BulkNack)
        .WillOnce([&](std::vector<std::string> const& ack_ids, std::size_t) {
          EXPECT_THAT(ack_ids, ElementsAre("ack-0-1", "ack-0-2"));
          return make_ready_future(Status{});
        });
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut =
      SubscriptionFlowControl::Create(background.cq(), shutdown, mock,
                                      /*message_count_lwm=*/0,
                                      /*message_count_hwm=*/10,
                                      /*message_size_lwm=*/kDefaultSizeLwm,
                                      /*message_size_hwm=*/kDefaultSizeHwm);

  auto done = shutdown->Start({});
  uut->Start([](google::pubsub::v1::ReceivedMessage const&) {});
  uut->Read(1);
  WaitAsyncPullCount(1);
  async_pulls_[0].set_value();
  WaitAsyncPullCount(2);
  // Errors are reported to the ShutdownManager by the LeaseManagement layer,
  // so let's do that here.
  shutdown->MarkAsShutdown("test-code", Status(StatusCode::kUnknown, "uh?"));
  // And then simulate the bulk cancel.
  async_pulls_[1].set_value();

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kUnknown, "uh?"));
}

TEST_F(SubscriptionFlowControlTest, ReachMessageCountHwm) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);
  EXPECT_CALL(*mock, BulkNack)
      .WillRepeatedly([](std::vector<std::string> const&, std::size_t) {
        return make_ready_future(Status{});
      });

  auto generate_messages = [](std::string const& prefix, int count) {
    return [prefix, count](future<void>) {
      return make_status_or(GenerateMessages(prefix, count));
    };
  };

  EXPECT_CALL(*mock, Pull)
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(10, max_messages);
        return AddAsyncPull().then(generate_messages("0-", 5));
      })
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(5, max_messages);
        return AddAsyncPull().then(generate_messages("1-", 5));
      })
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(8, max_messages);
        return AddAsyncPull().then(generate_messages("2-", 3));
      })
      .WillOnce([&](std::int32_t max_messages) {
        EXPECT_EQ(5, max_messages);
        return AddAsyncPull().then(generate_messages("3-", 5));
      });
  EXPECT_CALL(*mock, NackMessage)
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-0-1", ack_id);
        return make_ready_future(Status{});
      })
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-0-3", ack_id);
        return make_ready_future(Status{});
      })
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-1-1", ack_id);
        return make_ready_future(Status{});
      });

  EXPECT_CALL(*mock, AckMessage)
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-0-0", ack_id);
        return make_ready_future(Status{});
      })
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-0-2", ack_id);
        return make_ready_future(Status{});
      })
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-0-4", ack_id);
        return make_ready_future(Status{});
      })
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-1-0", ack_id);
        return make_ready_future(Status{});
      })
      .WillOnce([&](std::string const& ack_id, std::size_t) {
        EXPECT_EQ("ack-1-2", ack_id);
        return make_ready_future(Status{});
      });

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut = SubscriptionFlowControl::Create(
      background.cq(), shutdown, mock, /*message_count_lwm=*/2,
      /*message_count_hwm=*/10,
      /*message_size_lwm=*/kDefaultSizeLwm,
      /*message_size_hwm=*/kDefaultSizeHwm);

  auto callback = [&](google::pubsub::v1::ReceivedMessage const& response) {
    this->SaveAcks(response);
  };

  auto done = shutdown->Start({});
  uut->Start(callback);
  uut->Read(kUnlimitedCallbacks);
  WaitAsyncPullCount(1);
  EXPECT_TRUE(acks_.empty());
  async_pulls_[0].set_value();
  WaitAcksCount(5);
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-0-0", "ack-0-1", "ack-0-2",
                                         "ack-0-3", "ack-0-4"));
  WaitAsyncPullCount(2);
  async_pulls_[1].set_value();
  WaitAcksCount(5);
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-1-0", "ack-1-1", "ack-1-2",
                                         "ack-1-3", "ack-1-4"));
  // Acking or Nacking the first 7 messages should have no effect, no new calls
  // or incoming data.
  for (auto const* id : {"ack-0-0", "ack-0-2", "ack-0-4", "ack-1-0"}) {
    uut->AckMessage(id, 0);
  }
  for (auto const* id : {"ack-0-1", "ack-0-3", "ack-1-1"}) {
    uut->NackMessage(id, 0);
  }
  EXPECT_EQ(2, async_pulls_.size());

  // Acking message number 8 should bring the count of messages below the LWM
  // and trigger a new AsyncPull(), the third expectation from above.
  uut->AckMessage("ack-1-2", 0);
  WaitAsyncPullCount(3);
  async_pulls_[2].set_value();
  WaitAcksCount(3);
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-2-0", "ack-2-1", "ack-2-2"));

  WaitAsyncPullCount(4);

  // Shutting things down should prevent any further calls.
  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();
  // Only after shutdown complete the last AsyncPull().
  async_pulls_[3].set_value();

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

TEST_F(SubscriptionFlowControlTest, ReachMessageSizeHwm) {
  auto mock = std::make_shared<pubsub_testing::MockSubscriptionBatchSource>();
  EXPECT_CALL(*mock, Shutdown);

  auto constexpr kMessageSize = 1024;
  auto generate_messages = [&](std::string const& prefix, int count) {
    // Workaround GCC/Clang warnings for capturing constexpr vs. MSVC behavior
    // around not capturing without defaults.
    auto const size = kMessageSize;
    return [=](future<void>) {
      return make_status_or(GenerateSizedMessages(prefix, count, size));
    };
  };

  auto ack_success = [&](std::string const&, std::size_t) {
    return make_ready_future(Status{});
  };

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(*mock, Pull(20)).WillOnce([&](std::int32_t) {
      return AddAsyncPull().then(generate_messages("0-", 5));
    });
    EXPECT_CALL(*mock, Pull(15)).WillOnce([&](std::int32_t) {
      return AddAsyncPull().then(generate_messages("1-", 5));
    });
    EXPECT_CALL(*mock, AckMessage("ack-0-0", 1024)).WillOnce(ack_success);
    EXPECT_CALL(*mock, AckMessage("ack-0-2", 1024)).WillOnce(ack_success);
    EXPECT_CALL(*mock, Pull(12)).WillOnce([&](std::int32_t) {
      return AddAsyncPull().then(generate_messages("2-", 3));
    });
  }

  google::cloud::internal::AutomaticallyCreatedBackgroundThreads background;
  auto shutdown = std::make_shared<SessionShutdownManager>();
  auto uut =
      SubscriptionFlowControl::Create(background.cq(), shutdown, mock,
                                      /*message_count_lwm=*/19,
                                      /*message_count_hwm=*/20,
                                      /*message_size_lwm=*/8 * kMessageSize,
                                      /*message_size_hwm=*/10 * kMessageSize);

  auto callback = [&](google::pubsub::v1::ReceivedMessage const& response) {
    this->SaveAcks(response);
  };

  auto done = shutdown->Start({});
  uut->Start(callback);
  uut->Read(kUnlimitedCallbacks);
  WaitAsyncPullCount(1);
  EXPECT_TRUE(acks_.empty());
  async_pulls_[0].set_value();
  WaitAcksCount(5);
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-0-0", "ack-0-1", "ack-0-2",
                                         "ack-0-3", "ack-0-4"));
  WaitAsyncPullCount(2);
  async_pulls_[1].set_value();
  WaitAcksCount(5);
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-1-0", "ack-1-1", "ack-1-2",
                                         "ack-1-3", "ack-1-4"));

  // We are below the HWM for message count, and above the HWM for message size.
  // That means no more calls until we go below the HWM for mesages.
  uut->AckMessage("ack-0-0", 1024);
  uut->AckMessage("ack-0-2", 1024);
  WaitAsyncPullCount(3);
  async_pulls_[2].set_value();
  WaitAcksCount(3);
  EXPECT_THAT(CurrentAcks(), ElementsAre("ack-2-0", "ack-2-1", "ack-2-2"));

  // Shutting things down should prevent any further calls.
  shutdown->MarkAsShutdown(__func__, {});
  uut->Shutdown();

  EXPECT_THAT(done.get(), StatusIs(StatusCode::kOk));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
