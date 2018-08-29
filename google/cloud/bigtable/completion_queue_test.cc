// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gmock/gmock.h>
#include <future>

using namespace google::cloud::testing_util::chrono_literals;
namespace btproto = google::bigtable::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

/// @test Verify the basic lifecycle of a completion queue.
TEST(CompletionQueueTest, LifeCycle) {
  CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  std::promise<bool> promise;
  auto alarm = cq.MakeRelativeTimer(
      2_ms, [&promise](AsyncTimerResult&, bool) { promise.set_value(true); });

  auto f = promise.get_future();
  auto status = f.wait_for(50_ms);
  EXPECT_EQ(std::future_status::ready, status);

  cq.Shutdown();
  t.join();
}

/// @test Verify that we can cancel alarms.
TEST(CompletionQueueTest, CancelAlarm) {
  bigtable::CompletionQueue cq;

  std::thread t([&cq]() { cq.Run(); });

  std::promise<bool> promise;
  auto alarm = cq.MakeRelativeTimer(
      50_ms, [&promise](AsyncTimerResult&, bool ok) { promise.set_value(ok); });

  alarm->Cancel();

  auto f = promise.get_future();
  auto status = f.wait_for(100_ms);
  EXPECT_EQ(std::future_status::ready, status);
  EXPECT_FALSE(f.get());

  cq.Shutdown();
  t.join();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
