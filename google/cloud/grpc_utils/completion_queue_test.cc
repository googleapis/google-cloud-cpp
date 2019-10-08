// Copyright 2019 Google LLC
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

#include "google/cloud/grpc_utils/completion_queue.h"
#include "google/cloud/future.h"
#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>

namespace google {
namespace cloud {
namespace grpc_utils {
inline namespace GOOGLE_CLOUD_CPP_GRPC_UTILS_NS {
namespace {

class MockCompletionQueue : public internal::CompletionQueueImpl {
 public:
  using internal::CompletionQueueImpl::SimulateCompletion;
};

/// @test Verify that the basic functionality in a CompletionQueue works.
TEST(CompletionQueueTest, TimerSmokeTest) {
  CompletionQueue cq;
  std::thread t([&cq] { cq.Run(); });

  using ms = std::chrono::milliseconds;
  promise<void> wait_for_sleep;
  cq.MakeRelativeTimer(ms(2))
      .then([&wait_for_sleep](future<std::chrono::system_clock::time_point>) {
        wait_for_sleep.set_value();
      })
      .get();

  auto f = wait_for_sleep.get_future();
  EXPECT_EQ(std::future_status::ready, f.wait_for(ms(0)));
  cq.Shutdown();
  t.join();
}

TEST(CompletionQueueTest, MockSmokeTest) {
  auto mock = std::make_shared<MockCompletionQueue>();

  CompletionQueue cq(mock);
  using ms = std::chrono::milliseconds;
  promise<void> wait_for_sleep;
  cq.MakeRelativeTimer(ms(20000)).then(
      [&wait_for_sleep](future<std::chrono::system_clock::time_point>) {
        wait_for_sleep.set_value();
      });
  mock->SimulateCompletion(cq, true);

  auto f = wait_for_sleep.get_future();
  EXPECT_EQ(std::future_status::ready, f.wait_for(ms(0)));
  cq.Shutdown();
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GRPC_UTILS_NS
}  // namespace grpc_utils
}  // namespace cloud
}  // namespace google
