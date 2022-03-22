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
using ::testing::Return;
using ::testing::StrictMock;

TEST(AlarmRegistryImpl, TokenDestroyedBeforeRun) {
  auto cq = std::make_shared<StrictMock<MockCompletionQueueImpl>>();
  AlarmRegistryImpl alarm{CompletionQueue{cq}};
  bool flag = false;
  auto fun = [&flag]() { flag = true; };
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(1000000)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token =
      alarm.RegisterAlarm(std::chrono::milliseconds(1), std::move(fun));
  token = nullptr;
  timer.set_value(std::chrono::system_clock::time_point{});
  EXPECT_FALSE(flag);
}

TEST(AlarmRegistryImpl, TimerErrorBeforeRun) {
  auto cq = std::make_shared<StrictMock<MockCompletionQueueImpl>>();
  AlarmRegistryImpl alarm{CompletionQueue{cq}};
  bool flag = false;
  auto fun = [&flag]() { flag = true; };
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(1000000)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token =
      alarm.RegisterAlarm(std::chrono::milliseconds(1), std::move(fun));
  timer.set_value(Status{StatusCode::kCancelled, "cancelled"});
  EXPECT_FALSE(flag);
}

TEST(AlarmRegistryImpl, TokenDestroyedAfterSingleRun) {
  InSequence seq;

  auto cq = std::make_shared<StrictMock<MockCompletionQueueImpl>>();
  AlarmRegistryImpl alarm{CompletionQueue{cq}};
  bool flag = false;
  auto fun = [&flag]() { flag = !flag; };
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(1000000)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token =
      alarm.RegisterAlarm(std::chrono::milliseconds(1), std::move(fun));

  auto temp_promise = std::move(timer);
  timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};
  EXPECT_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(1000000)))
      .WillOnce(Return(ByMove(timer.get_future())));
  temp_promise.set_value(std::chrono::system_clock::time_point{});

  token = nullptr;

  timer.set_value(std::chrono::system_clock::time_point{});
  EXPECT_TRUE(flag);
}

TEST(AlarmRegistryImpl, TokenDestroyedAfterFiveRuns) {
  InSequence seq;

  auto cq = std::make_shared<StrictMock<MockCompletionQueueImpl>>();
  AlarmRegistryImpl alarm{CompletionQueue{cq}};
  int ctr = 0;
  auto fun = [&ctr]() { ++ctr; };
  promise<StatusOr<std::chrono::system_clock::time_point>> timer;
  EXPECT_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(1000000)))
      .WillOnce(Return(ByMove(timer.get_future())));
  auto token =
      alarm.RegisterAlarm(std::chrono::milliseconds(1), std::move(fun));

  for (unsigned int i = 0; i < 5; ++i) {
    auto temp_promise = std::move(timer);
    timer = promise<StatusOr<std::chrono::system_clock::time_point>>{};
    EXPECT_CALL(*cq, MakeRelativeTimer(std::chrono::nanoseconds(1000000)))
        .WillOnce(Return(ByMove(timer.get_future())));
    temp_promise.set_value(std::chrono::system_clock::time_point{});
  }

  token = nullptr;

  timer.set_value(std::chrono::system_clock::time_point{});
  EXPECT_EQ(ctr, 5);
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
