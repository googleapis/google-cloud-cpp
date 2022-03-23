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

#include "google/cloud/pubsublite/internal/alarm_registry_impl.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include <gmock/gmock.h>
#include <chrono>
#include <deque>
#include <memory>
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using google::cloud::CompletionQueue;
using google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::ByMove;
using ::testing::InSequence;
;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

// 10,000 seconds to ensure this test has no time dependence
std::chrono::milliseconds constexpr kAlarmPeriod{10000 * 1000};

class AlarmRegistryImplTest : public ::testing::Test {
 protected:
  AlarmRegistryImplTest()
      : cq_{std::make_shared<StrictMock<MockCompletionQueueImpl>>()},
        alarm_{CompletionQueue{cq_}} {}
  std::shared_ptr<StrictMock<MockCompletionQueueImpl>> cq_;
  AlarmRegistryImpl alarm_;
  StrictMock<MockFunction<void()>> fun_;
};

TEST_F(AlarmRegistryImplTest, TokenDestroyedBeforeRun) {
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());
  token = nullptr;
  timer.set_value(std::chrono::system_clock::time_point{});
}

TEST_F(AlarmRegistryImplTest, TimerErrorBeforeRun) {
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());
  timer.set_value(Status{StatusCode::kCancelled, "cancelled"});
}

TEST_F(AlarmRegistryImplTest, TokenDestroyedAfterSingleRun) {
  InSequence seq;
  bool flag = false;
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());

  EXPECT_CALL(fun_, Call).WillOnce([&flag]() { flag = !flag; });

  auto temp_promise = std::move(timer);
  timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  temp_promise.set_value(std::chrono::system_clock::time_point{});

  token = nullptr;

  timer.set_value(std::chrono::system_clock::time_point{});
  EXPECT_TRUE(flag);
}

TEST_F(AlarmRegistryImplTest, TokenDestroyedAfterFiveRuns) {
  InSequence seq;

  int ctr = 0;
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());

  for (unsigned int i = 0; i < 5; ++i) {
    EXPECT_CALL(fun_, Call).WillOnce([&ctr]() { ++ctr; });
    auto temp_promise = std::move(timer);
    timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};
    EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
        .WillOnce(Return(ByMove(timer.get_future())));
    temp_promise.set_value(std::chrono::system_clock::time_point{});
  }

  token = nullptr;

  timer.set_value(std::chrono::system_clock::time_point{});
  EXPECT_EQ(ctr, 5);
}

TEST(AlarmRegistryImpl, TokenDestroyedDuringSecondRun) {
  InSequence seq;

  // not using StrictMock because of ON_CALL
  auto cq = std::make_shared<MockCompletionQueueImpl>();
  AlarmRegistryImpl alarm{CompletionQueue{cq}};
  StrictMock<MockFunction<void()>> fun;

  bool flag = false;
  // using ctr to assert that MakeRelativeTimer is called at least twice
  int ctr = 0;
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;

  // using ON_CALL due to nondeterminism with which thread acquires the lock
  // first when destructor is invoked
  ON_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillByDefault([&timer, &ctr]() {
        ++ctr;
        return timer.get_future();
      });

  auto token = alarm.RegisterAlarm(kAlarmPeriod, fun.AsStdFunction());

  EXPECT_EQ(ctr, 1);

  EXPECT_CALL(fun, Call).WillOnce([&flag]() { flag = !flag; });

  auto temp_promise = std::move(timer);
  timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};
  temp_promise.set_value(std::chrono::system_clock::time_point{});

  EXPECT_EQ(ctr, 2);

  EXPECT_TRUE(flag);

  promise<void> in_alarm_function;
  promise<void> about_to_destroy;
  auto destroy_token = [&in_alarm_function, &about_to_destroy, &token]() {
    // guarantee that alarm function is being invoked
    in_alarm_function.get_future().get();
    about_to_destroy.set_value();
    token = nullptr;
  };

  EXPECT_CALL(fun, Call).WillOnce(
      [&in_alarm_function, &about_to_destroy, &flag]() {
        in_alarm_function.set_value();
        about_to_destroy.get_future().get();
        flag = !flag;
      });

  temp_promise = std::move(timer);
  timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};

  std::thread first{destroy_token};

  temp_promise.set_value(std::chrono::system_clock::time_point{});

  first.join();

  timer.set_value(std::chrono::system_clock::time_point{});

  // second run has to complete
  EXPECT_FALSE(flag);
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
