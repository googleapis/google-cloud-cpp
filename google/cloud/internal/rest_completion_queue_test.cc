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

#include "google/cloud/internal/rest_completion_queue.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(RestCompletionQueue, AddTag) {
  int tag;
  RestCompletionQueue cq;
  ASSERT_EQ(cq.size(), 0);
  cq.AddTag(static_cast<void*>(&tag));
  EXPECT_EQ(cq.size(), 1);
}

TEST(RestCompletionQueue, RemoveTag) {
  int tag1;
  int tag2;
  RestCompletionQueue cq;
  ASSERT_EQ(cq.size(), 0);
  cq.AddTag(static_cast<void*>(&tag1));
  EXPECT_EQ(cq.size(), 1);
  cq.RemoveTag(static_cast<void*>(&tag2));
  EXPECT_EQ(cq.size(), 1);
  cq.RemoveTag(static_cast<void*>(&tag1));
  EXPECT_EQ(cq.size(), 0);
}

TEST(RestCompletionQueue, GetNext) {
  int tag1;
  RestCompletionQueue cq;
  ASSERT_EQ(cq.size(), 0);
  cq.AddTag(static_cast<void*>(&tag1));
  void* tag;
  bool ok;
  auto status = cq.GetNext(&tag, &ok, std::chrono::system_clock::now());
  EXPECT_EQ(status, RestCompletionQueue::QueueStatus::kGotEvent);
  EXPECT_EQ(tag, static_cast<void*>(&tag1));
  EXPECT_TRUE(ok);
  status = cq.GetNext(&tag, &ok, std::chrono::system_clock::now());
  EXPECT_EQ(status, RestCompletionQueue::QueueStatus::kTimeout);
}

TEST(RestCompletionQueue, ShutdownThenGetNext) {
  int tag1;
  RestCompletionQueue cq;
  ASSERT_EQ(cq.size(), 0);
  cq.AddTag(static_cast<void*>(&tag1));
  cq.Shutdown();
  void* tag;
  bool ok;
  auto status = cq.GetNext(&tag, &ok, std::chrono::system_clock::now());
  EXPECT_EQ(status, RestCompletionQueue::QueueStatus::kShutdown);
  EXPECT_EQ(tag, nullptr);
  EXPECT_FALSE(ok);
}

TEST(RestCompletionQueue, ShutdownThenAddTag) {
  int tag1;
  RestCompletionQueue cq;
  ASSERT_EQ(cq.size(), 0);
  cq.Shutdown();
  cq.AddTag(static_cast<void*>(&tag1));
  EXPECT_EQ(cq.size(), 0);
}

TEST(RestCompletionQueue, ShutdownThenRemoveTag) {
  int tag1;
  int tag2;
  RestCompletionQueue cq;
  ASSERT_EQ(cq.size(), 0);
  cq.AddTag(static_cast<void*>(&tag1));
  cq.AddTag(static_cast<void*>(&tag2));
  EXPECT_EQ(cq.size(), 2);
  cq.Shutdown();
  cq.RemoveTag(static_cast<void*>(&tag1));
  EXPECT_EQ(cq.size(), 0);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
