// Copyright 2026 Google LLC
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

#include "google/cloud/bigtable/internal/dynamic_channel_pool.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class DynamicChannelPoolTestWrapper {
 public:
  explicit DynamicChannelPoolTestWrapper(
      std::shared_ptr<DynamicChannelPool<BigtableStub>> pool)
      : pool_(std::move(pool)) {}

  void ScheduleAddChannels(
      std::scoped_lock<std::mutex> const& lk,
      std::function<void(std::vector<int> const&)> const& test_fn) {
    pool_->ScheduleAddChannels(lk, test_fn);
  }

  void AddChannels(std::vector<int> const& new_channel_ids) {
    pool_->AddChannels(new_channel_ids);
  }

  void ScheduleRemoveChannels(std::scoped_lock<std::mutex> const& lk) {
    pool_->ScheduleRemoveChannels(lk);
  }

  void RemoveChannels() { pool_->RemoveChannels(); }

  void SetSizeDecreaseCooldownTimer(std::scoped_lock<std::mutex> const& lk) {
    pool_->SetSizeDecreaseCooldownTimer(lk);
  }

  std::scoped_lock<std::mutex> CreateLock() {
    return std::scoped_lock(pool_->mu_);
  }

  void SetRemoveChannelPollTimer(promise<void>& p) {
    pool_->remove_channel_poll_timer_ = p.get_future();
  }

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> const&
  SetDrainingChannels(std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>>
                          draining_channels) {
    pool_->draining_channels_ = std::move(draining_channels);
    return pool_->draining_channels_;
  }

  std::size_t num_pending_channels() const {
    return pool_->num_pending_channels_;
  }

  DynamicChannelPoolTestWrapper& set_num_pending_channels(std::size_t n) {
    pool_->num_pending_channels_ = n;
    return *this;
  }

 protected:
  std::shared_ptr<DynamicChannelPool<BigtableStub>> pool_;
};

namespace {

using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::MockFunction;

class DynamicChannelPoolTest : public ::testing::Test {
 public:
  DynamicChannelPoolTest()
      : fake_cq_impl_(std::make_shared<FakeCompletionQueueImpl>()),
        mock_cq_impl_(std::make_shared<MockCompletionQueueImpl>()),
        thread_([this] { cq_.Run(); }) {}

  ~DynamicChannelPoolTest() override {
    cq_.Shutdown();
    thread_.join();
  }

 protected:
  std::shared_ptr<FakeCompletionQueueImpl> fake_cq_impl_;
  std::shared_ptr<MockCompletionQueueImpl> mock_cq_impl_;
  CompletionQueue cq_;
  std::thread thread_;
};

TEST_F(DynamicChannelPoolTest, ScheduleAddChannelsPoolUndersized) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  DynamicChannelPoolSizingPolicy sizing_policy;
  sizing_policy.minimum_channel_pool_size = 10;

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  {
    EXPECT_CALL(*mock_cq_impl_, RunAsync).Times(1);
    auto test_fn = [](std::vector<int> const& new_channel_ids) {
      EXPECT_THAT(new_channel_ids,
                  ::testing::ElementsAreArray({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
    };
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleAddChannels(lk, test_fn);
    EXPECT_THAT(wrapper.num_pending_channels(), Eq(10));
  }

  {
    EXPECT_CALL(*mock_cq_impl_, RunAsync).Times(1);
    auto test_fn = [](std::vector<int> const& new_channel_ids) {
      EXPECT_THAT(new_channel_ids,
                  ::testing::ElementsAreArray(
                      {10, 11, 12, 13, 14, 15, 16, 17, 18, 19}));
    };
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleAddChannels(lk, test_fn);
    EXPECT_THAT(wrapper.num_pending_channels(), Eq(20));
  }
}

TEST_F(DynamicChannelPoolTest, ScheduleAddChannelsPoolNearMax) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  DynamicChannelPoolSizingPolicy sizing_policy;
  sizing_policy.minimum_channel_pool_size = 2;
  sizing_policy.maximum_channel_pool_size = 3;
  sizing_policy.channels_to_add_per_resize =
      DynamicChannelPoolSizingPolicy::DiscreteChannels(3);

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  EXPECT_CALL(*mock_cq_impl_, RunAsync).Times(1);
  auto test_fn = [](std::vector<int> const& new_channel_ids) {
    EXPECT_THAT(new_channel_ids, ::testing::ElementsAreArray({2}));
  };

  {
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleAddChannels(lk, test_fn);
  }
}

TEST_F(DynamicChannelPoolTest, ScheduleAddChannelsPoolNotNearMax) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  DynamicChannelPoolSizingPolicy sizing_policy;
  sizing_policy.minimum_channel_pool_size = 2;
  sizing_policy.maximum_channel_pool_size = 10;
  sizing_policy.channels_to_add_per_resize =
      DynamicChannelPoolSizingPolicy::DiscreteChannels(3);

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  EXPECT_CALL(*mock_cq_impl_, RunAsync).Times(1);
  auto test_fn = [](std::vector<int> const& new_channel_ids) {
    EXPECT_THAT(new_channel_ids, ::testing::ElementsAreArray({2, 3, 4}));
  };

  {
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleAddChannels(lk, test_fn);
  }
}

TEST_F(DynamicChannelPoolTest, ScheduleAddChannelsPoolNotNearMaxPercentage) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  DynamicChannelPoolSizingPolicy sizing_policy;
  sizing_policy.minimum_channel_pool_size = 2;
  sizing_policy.maximum_channel_pool_size = 10;
  sizing_policy.channels_to_add_per_resize =
      DynamicChannelPoolSizingPolicy::PercentageOfPoolSize(0.5);

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  EXPECT_CALL(*mock_cq_impl_, RunAsync).Times(1);
  auto test_fn = [](std::vector<int> const& new_channel_ids) {
    EXPECT_THAT(new_channel_ids, ::testing::ElementsAreArray({2}));
  };

  {
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleAddChannels(lk, test_fn);
  }
}

TEST_F(DynamicChannelPoolTest, AddChannels) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  DynamicChannelPoolSizingPolicy sizing_policy;
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;
  auto mock_stub_0 = std::make_shared<MockBigtableStub>();
  auto mock_stub_1 = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(stub_factory_fn, Call)
      .WillOnce([&](int id, std::string const& instance,
                    StubManager::Priming priming) {
        EXPECT_THAT(id, Eq(0));
        EXPECT_THAT(instance, Eq(instance_name));
        EXPECT_THAT(priming, Eq(StubManager::Priming::kSynchronousPriming));
        return std::make_shared<ChannelUsage<BigtableStub>>(mock_stub_0, 20);
      })
      .WillOnce([&](int id, std::string const& instance,
                    StubManager::Priming priming) {
        EXPECT_THAT(id, Eq(1));
        EXPECT_THAT(instance, Eq(instance_name));
        EXPECT_THAT(priming, Eq(StubManager::Priming::kSynchronousPriming));
        return std::make_shared<ChannelUsage<BigtableStub>>(mock_stub_1, 20);
      });

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);
  std::vector<int> new_channel_ids = {0, 1};
  wrapper.set_num_pending_channels(new_channel_ids.size());
  wrapper.AddChannels(new_channel_ids);
  EXPECT_THAT(pool->size(), Eq(2));
  EXPECT_THAT(wrapper.num_pending_channels(), Eq(0));
  fake_cq_impl_->SimulateCompletion(false);
}

TEST_F(DynamicChannelPoolTest, ScheduleRemoveChannelsAlreadyPending) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  DynamicChannelPoolSizingPolicy sizing_policy;

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  promise<void> p;
  wrapper.SetRemoveChannelPollTimer(p);
  {
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleRemoveChannels(lk);
  }

  fake_cq_impl_->SimulateCompletion(false);
}

TEST_F(DynamicChannelPoolTest, ScheduleRemoveChannelsNotAlreadyPending) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  DynamicChannelPoolSizingPolicy sizing_policy;

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.remove_channel_polling_interval)
                           .count()));
        return cq_.MakeRelativeTimer(std::chrono::seconds(600));
      });
  {
    auto lk = wrapper.CreateLock();
    wrapper.ScheduleRemoveChannels(lk);
  }

  fake_cq_impl_->SimulateCompletion(false);
}

TEST_F(DynamicChannelPoolTest, RemoveChannelsLoneChannelDrained) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  DynamicChannelPoolSizingPolicy sizing_policy;

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> draining_channels;
  draining_channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  auto const& d = wrapper.SetDrainingChannels(draining_channels);

  wrapper.RemoveChannels();
  EXPECT_THAT(d, IsEmpty());

  fake_cq_impl_->SimulateCompletion(false);
}

TEST_F(DynamicChannelPoolTest, RemoveChannelsSomeChannelsDrained) {
  auto instance_name =
      bigtable::InstanceResource(Project("my-project"), "my-instance")
          .FullName();
  auto refresh_state = std::make_shared<ConnectionRefreshState>(
      fake_cq_impl_, std::chrono::milliseconds(1),
      std::chrono::milliseconds(10));

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> channels;
  DynamicChannelPoolSizingPolicy sizing_policy;

  MockFunction<StatusOr<std::shared_ptr<ChannelUsage<BigtableStub>>>(
      std::uint32_t, std::string const&, StubManager::Priming)>
      stub_factory_fn;

  // Pool creation should set the pool size increase cooldown timer.
  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.pool_size_decrease_cooldown_interval)
                           .count()));
        return make_ready_future(StatusOr(std::chrono::system_clock::now()));
      });

  auto pool = DynamicChannelPool<BigtableStub>::Create(
      instance_name, CompletionQueue(mock_cq_impl_), channels, refresh_state,
      stub_factory_fn.AsStdFunction(), sizing_policy);
  DynamicChannelPoolTestWrapper wrapper(pool);

  std::vector<std::shared_ptr<ChannelUsage<BigtableStub>>> draining_channels;
  draining_channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 1));
  draining_channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  draining_channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 0));
  draining_channels.push_back(std::make_shared<ChannelUsage<BigtableStub>>(
      std::make_shared<MockBigtableStub>(), 2));
  auto const& d = wrapper.SetDrainingChannels(draining_channels);

  EXPECT_CALL(*mock_cq_impl_, MakeRelativeTimer)
      .WillOnce([&](std::chrono::nanoseconds ns) {
        EXPECT_THAT(ns.count(),
                    Eq(std::chrono::nanoseconds(
                           sizing_policy.remove_channel_polling_interval)
                           .count()));
        // Set a timer that will NOT finish before it is cancelled by the test.
        return cq_.MakeRelativeTimer(std::chrono::seconds(600));
      });

  wrapper.RemoveChannels();
  ASSERT_THAT(d.size(), Eq(2));
  EXPECT_THAT(d[0]->instant_outstanding_rpcs(), testing_util::IsOkAndHolds(2));
  EXPECT_THAT(d[1]->instant_outstanding_rpcs(), testing_util::IsOkAndHolds(1));

  fake_cq_impl_->SimulateCompletion(false);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
