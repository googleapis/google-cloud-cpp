// Copyright 2021 Google LLC
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

#include "google/cloud/testing_util/async_sequencer.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {
namespace {

TEST(AsyncSequencerTest, Simple) {
  AsyncSequencer<int> async;
  auto f = async.PushBack();
  EXPECT_FALSE(f.is_ready());

  auto p = async.PopFront();
  p.set_value(42);

  EXPECT_TRUE(f.is_ready());
  EXPECT_EQ(f.get(), 42);
}

TEST(AsyncSequencerTest, AnyOrder) {
  AsyncSequencer<void> async;
  auto f0 = async.PushBack().then([](future<void>) { return 42; });
  auto f1 = async.PushBack().then([](future<void>) { return 84; });
  auto f2 = async.PushBack().then([](future<void>) { return 21; });

  auto p0 = async.PopFront();
  auto p1 = async.PopFront();
  auto p2 = async.PopFront();

  // Satisfy the futures out of order
  p2.set_value();
  EXPECT_EQ(f2.get(), 21);
  p0.set_value();
  EXPECT_EQ(f0.get(), 42);
  p1.set_value();
  EXPECT_EQ(f1.get(), 84);
}

TEST(AsyncSequencerTest, WithName) {
  AsyncSequencer<void> async;
  (void)async.PushBack("test");
  auto pair = async.PopFrontWithName();
  EXPECT_EQ(pair.second, "test");
}

TEST(AsyncSequencerTest, CancelCount) {
  AsyncSequencer<void> async;
  auto f1 = async.PushBack();
  auto f2 = async.PushBack();
  EXPECT_EQ(0, async.CancelCount());
  f1.cancel();
  f2.cancel();
  EXPECT_EQ(2, async.CancelCount());
  // Verify that the counter resets to 0 after each call.
  EXPECT_EQ(0, async.CancelCount());
}

TEST(AsyncSequencerTest, MaxSize) {
  AsyncSequencer<void> async;
  EXPECT_EQ(0, async.MaxSize());
  (void)async.PushBack();
  EXPECT_EQ(1, async.MaxSize());
  (void)async.PushBack();
  EXPECT_EQ(2, async.MaxSize());
  (void)async.PopFront();
  (void)async.PushBack();
  EXPECT_EQ(2, async.MaxSize());
}

}  // namespace
}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
