// Copyright 2023 Google LLC
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

#include "google/cloud/internal/rest_background_threads_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Not;

/// @test Verify that automatically created completion queues are usable.
TEST(AutomaticallyCreatedRestBackgroundThreads, IsActive) {
  AutomaticallyCreatedRestBackgroundThreads actual;
  EXPECT_EQ(1, actual.pool_size());

  promise<std::thread::id> bg;
  actual.cq().RunAsync([&bg] { bg.set_value(std::this_thread::get_id()); });
  EXPECT_NE(std::this_thread::get_id(), bg.get_future().get());
}

/// @test Verify that automatically created completion queues are usable.
TEST(AutomaticallyCreatedRestBackgroundThreads, NoEmptyPools) {
  AutomaticallyCreatedRestBackgroundThreads actual(0);
  EXPECT_EQ(1, actual.pool_size());

  promise<std::thread::id> bg;
  actual.cq().RunAsync([&bg] { bg.set_value(std::this_thread::get_id()); });
  EXPECT_NE(std::this_thread::get_id(), bg.get_future().get());
}

/// @test Verify that automatically created completion queues work.
TEST(AutomaticallyCreatedRestBackgroundThreads, ManyThreads) {
  auto constexpr kThreadCount = 4;
  AutomaticallyCreatedRestBackgroundThreads actual(kThreadCount);
  EXPECT_EQ(kThreadCount, actual.pool_size());

  std::vector<promise<std::thread::id>> promises(100 * kThreadCount);
  for (auto& p : promises) {
    actual.cq().RunAsync([&p] { p.set_value(std::this_thread::get_id()); });
  }
  std::set<std::thread::id> ids;
  for (auto& p : promises) ids.insert(p.get_future().get());
  EXPECT_FALSE(ids.empty());
  EXPECT_GE(kThreadCount, ids.size());
  EXPECT_THAT(ids, Not(Contains(std::this_thread::get_id())));
}

/// @test Verify that automatically created completion queues work.
TEST(AutomaticallyCreatedRestBackgroundThreads, ManualShutdown) {
  auto constexpr kThreadCount = 4;
  AutomaticallyCreatedRestBackgroundThreads actual(kThreadCount);
  EXPECT_EQ(kThreadCount, actual.pool_size());

  std::vector<promise<void>> promises(2 * kThreadCount);
  for (auto& p : promises) {
    actual.cq().RunAsync([&p] { p.set_value(); });
  }
  for (auto& p : promises) p.get_future().get();
  actual.Shutdown();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
