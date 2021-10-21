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

#include "google/cloud/pubsub/internal/session_shutdown_manager.h"
#include "google/cloud/internal/background_threads_impl.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

/// @test Verify SessionShutdownManager works correctly.
TEST(SessionShutdownManagerTest, Basic) {
  internal::AutomaticallyCreatedBackgroundThreads background;
  CompletionQueue cq = background.cq();

  SessionShutdownManager tested;
  auto shutdown = tested.Start({});
  EXPECT_TRUE(tested.StartOperation("testing", "operation-1", [] {}));
  EXPECT_TRUE(tested.StartOperation("testing", "operation-2", [] {}));
  EXPECT_TRUE(
      tested.StartAsyncOperation("testing", "async-operation-1", cq, [] {}));
  EXPECT_TRUE(
      tested.StartAsyncOperation("testing", "async-operation-2", cq, [] {}));

  EXPECT_FALSE(tested.FinishedOperation("operation-1"));
  EXPECT_FALSE(tested.FinishedOperation("async-operation-1"));

  auto const expected_status = Status{StatusCode::kAborted, "test-message"};
  tested.MarkAsShutdown("testing", expected_status);
  EXPECT_EQ(std::future_status::timeout,
            shutdown.wait_for(std::chrono::milliseconds(0)));

  EXPECT_FALSE(
      tested.StartAsyncOperation("testing", "async-operation-3", cq, [] {}));
  EXPECT_FALSE(tested.StartOperation("testing", "operation-3", [] {}));
  EXPECT_EQ(std::future_status::timeout,
            shutdown.wait_for(std::chrono::milliseconds(0)));

  EXPECT_TRUE(tested.FinishedOperation("operation-2-mismatch-does-not-matter"));
  EXPECT_EQ(std::future_status::timeout,
            shutdown.wait_for(std::chrono::milliseconds(0)));
  EXPECT_TRUE(tested.FinishedOperation("async-operation-2"));

  EXPECT_EQ(expected_status, shutdown.get());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
