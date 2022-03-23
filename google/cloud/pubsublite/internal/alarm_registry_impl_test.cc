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

  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());

  EXPECT_CALL(fun_, Call);

  promise<StatusOr<std::chrono::system_clock::time_point>> timer1;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer1.get_future())));
  timer.set_value(std::chrono::system_clock::time_point{});

  token = nullptr;

  timer1.set_value(std::chrono::system_clock::time_point{});
}

TEST_F(AlarmRegistryImplTest, TokenDestroyedAfterFiveRuns) {
  InSequence seq;

  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());

  for (unsigned int i = 0; i < 5; ++i) {
    EXPECT_CALL(fun_, Call);
    auto temp_promise = std::move(timer);
    timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};
    EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
        .WillOnce(Return(ByMove(timer.get_future())));
    temp_promise.set_value(std::chrono::system_clock::time_point{});
  }

  token = nullptr;

  timer.set_value(std::chrono::system_clock::time_point{});
}

TEST_F(AlarmRegistryImplTest, TokenDestroyedDuringSecondRun) {
  InSequence seq;

  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token = alarm_.RegisterAlarm(kAlarmPeriod, fun_.AsStdFunction());

  EXPECT_CALL(fun_, Call);

  promise<StatusOr<std::chrono::system_clock::time_point>> timer1;
  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer1.get_future())));
  timer.set_value(std::chrono::system_clock::time_point{});

  promise<void> in_alarm_function;
  promise<void> destroy_finished;
  auto destroy_token = [&in_alarm_function, &destroy_finished, &token]() {
    // guarantee that alarm function is being invoked
    in_alarm_function.get_future().get();
    token = nullptr;
    destroy_finished.set_value();
  };

  EXPECT_CALL(fun_, Call).WillOnce([&in_alarm_function, &destroy_finished]() {
    in_alarm_function.set_value();
    // Unable to destroy token while the alarm is outstanding
    EXPECT_EQ(destroy_finished.get_future().wait_for(std::chrono::seconds(2)),
              std::future_status::timeout);
  });

  promise<StatusOr<std::chrono::system_clock::time_point>> timer2;

  std::thread first{destroy_token};

  EXPECT_CALL(*cq_, MakeRelativeTimer(std::chrono::nanoseconds(kAlarmPeriod)))
      .WillOnce(Return(ByMove(timer2.get_future())));
  timer1.set_value(std::chrono::system_clock::time_point{});

  first.join();

  timer2.set_value(std::chrono::system_clock::time_point{});
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
