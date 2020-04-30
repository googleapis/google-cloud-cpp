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

#include "google/cloud/internal/background_threads_impl.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

/// @test Verify we can create and use a CustomerSuppliedBackgroundThreads
/// without impacting the completion queue
TEST(CustomerSuppliedBackgroundThreads, LifecycleNoShutdown) {
  CompletionQueue cq;
  promise<void> p;
  std::thread t([&cq, &p] {
    cq.Run();
    p.set_value();
  });

  { CustomerSuppliedBackgroundThreads actual(cq); }

  using ms = std::chrono::milliseconds;

  auto has_shutdown = p.get_future();
  EXPECT_NE(std::future_status::ready, has_shutdown.wait_for(ms(2)));

  auto expired = cq.MakeRelativeTimer(ms(0));
  EXPECT_EQ(std::future_status::ready, expired.wait_for(ms(500)));

  cq.Shutdown();
  EXPECT_EQ(std::future_status::ready, has_shutdown.wait_for(ms(500)));

  t.join();
}

/// @test Verify that users can supply their own queue and threads.
TEST(CustomerSuppliedBackgroundThreads, SharesCompletionQueue) {
  CompletionQueue cq;

  CustomerSuppliedBackgroundThreads actual(cq);

  using ms = std::chrono::milliseconds;
  // Verify the completion queue is shared. Scheduling work in actual.cq() works
  // once a thread is blocked in cq.Run(). Start that thread after scheduling
  // the work to avoid flaky failures where the timer expires immediately.
  future<std::thread::id> id = actual.cq().MakeRelativeTimer(ms(1)).then(
      [](future<StatusOr<std::chrono::system_clock::time_point>>) {
        return std::this_thread::get_id();
      });
  std::thread t([&cq] { cq.Run(); });
  EXPECT_EQ(std::future_status::ready, id.wait_for(ms(500)));
  EXPECT_EQ(t.get_id(), id.get());

  cq.Shutdown();
  t.join();
}

/// @test Verify that automatically created completion queues are usable.
TEST(AutomaticallyCreatedBackgroundThreads, IsActive) {
  AutomaticallyCreatedBackgroundThreads actual;

  using ms = std::chrono::milliseconds;

  auto expired = actual.cq().MakeRelativeTimer(ms(0));
  EXPECT_EQ(std::future_status::ready, expired.wait_for(ms(500)));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
