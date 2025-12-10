// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/async/multi_stream_manager.h"
#include <gmock/gmock.h>
#include <memory>
#include <unordered_map>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

struct FakeRange {
  bool done = false;
  int finished = 0;
  bool IsDone() const { return done; }
  void OnFinish(Status const&) { ++finished; }
};

struct FakeStream : public StreamBase {
  void Cancel() override { ++cancelled; }
  int cancelled = 0;
  bool write_pending = false;
};

using Manager = MultiStreamManager<FakeStream, FakeRange>;

struct MultiStreamManagerTest : public ::testing::Test {
  static Manager MakeManager() { return Manager([] { return std::make_shared<FakeStream>(); }); }
};

}  // namespace

TEST(MultiStreamManagerTest, ConstructsWithFactoryAndHasOneStream) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  EXPECT_FALSE(mgr.Empty());
  EXPECT_EQ(mgr.Size(), 1u);
  auto it = mgr.GetLastStream();
  ASSERT_TRUE(it->stream);
}

TEST(MultiStreamManagerTest, ConstructsWithInitialStream) {
  auto initial = std::make_shared<FakeStream>();
  Manager mgr([] { return nullptr; }, initial);
  EXPECT_EQ(mgr.Size(), 1u);
  auto it = mgr.GetLastStream();
  EXPECT_EQ(it->stream, initial);
}

TEST(MultiStreamManagerTest, AddStreamAppendsAndGetLastReturnsNew) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  auto s1 = std::make_shared<FakeStream>();
  auto it1 = mgr.AddStream(s1);
  EXPECT_EQ(mgr.Size(), 2u);
  EXPECT_EQ(it1->stream.get(), s1.get());
  auto it_last = mgr.GetLastStream();
  EXPECT_EQ(it_last->stream.get(), s1.get());
}

TEST(MultiStreamManagerTest, GetLeastBusyPrefersFewestActiveRanges) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  
  // The manager starts with an initial stream (size 0). 
  // We must make it "busy" so it doesn't win the comparison against our test streams.
  auto it_init = mgr.GetLastStream();
  it_init->active_ranges.emplace(999, std::make_shared<FakeRange>());
  it_init->active_ranges.emplace(998, std::make_shared<FakeRange>());

  auto s1 = std::make_shared<FakeStream>();
  auto s2 = std::make_shared<FakeStream>();
  auto it1 = mgr.AddStream(s1);
  auto it2 = mgr.AddStream(s2);

  // s1 has 2 ranges.
  it1->active_ranges.emplace(1, std::make_shared<FakeRange>());
  it1->active_ranges.emplace(2, std::make_shared<FakeRange>());
  
  // s2 has 1 range.
  it2->active_ranges.emplace(3, std::make_shared<FakeRange>());
  
  auto it_least = mgr.GetLeastBusyStream();
  
  // Expect it2 (1 range) over it1 (2 ranges) and it_init (2 ranges).
  EXPECT_EQ(it_least, it2); 
  EXPECT_EQ(it_least->active_ranges.size(), 1u);
}

TEST(MultiStreamManagerTest, CleanupDoneRangesRemovesFinished) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  auto it = mgr.GetLastStream();
  auto r1 = std::make_shared<FakeRange>(); r1->done = false;
  auto r2 = std::make_shared<FakeRange>(); r2->done = true;
  auto r3 = std::make_shared<FakeRange>(); r3->done = true;
  it->active_ranges.emplace(1, r1);
  it->active_ranges.emplace(2, r2);
  it->active_ranges.emplace(3, r3);
  mgr.CleanupDoneRanges(it);
  EXPECT_EQ(it->active_ranges.size(), 1u);
  EXPECT_TRUE(it->active_ranges.count(1));
}

TEST(MultiStreamManagerTest, RemoveStreamAndNotifyRangesCallsOnFinish) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  auto it = mgr.GetLastStream();
  auto r1 = std::make_shared<FakeRange>();
  auto r2 = std::make_shared<FakeRange>();
  it->active_ranges.emplace(11, r1);
  it->active_ranges.emplace(22, r2);
  mgr.RemoveStreamAndNotifyRanges(it, Status());  // OK status
  EXPECT_EQ(mgr.Size(), 0u);
  EXPECT_EQ(r1->finished, 1);
  EXPECT_EQ(r2->finished, 1);
}

TEST(MultiStreamManagerTest, CancelAllInvokesCancel) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  auto s1 = std::make_shared<FakeStream>();
  auto s2 = std::make_shared<FakeStream>();
  mgr.AddStream(s1);
  mgr.AddStream(s2);
  mgr.CancelAll();
  EXPECT_EQ(s1->cancelled, 1);
  EXPECT_EQ(s2->cancelled, 1);
}

TEST(MultiStreamManagerTest, ReuseIdleStreamToBackMovesElement) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  // Capture the factory-created stream pointer (initial element)
  auto factory_ptr = mgr.GetLastStream()->stream.get();
  auto s1 = std::make_shared<FakeStream>();
  mgr.AddStream(s1);
  bool moved = mgr.ReuseIdleStreamToBack(
      [](Manager::Stream const& s) {
        auto fs = s.stream.get();
        return fs != nullptr && s.active_ranges.empty() && !fs->write_pending;
      });
  EXPECT_TRUE(moved);
  auto it_last = mgr.GetLastStream();
  // After move, the factory stream should be last
  EXPECT_EQ(it_last->stream.get(), factory_ptr);
  EXPECT_NE(it_last->stream.get(), s1.get());
}

TEST(MultiStreamManagerTest, ReuseIdleStreamAlreadyAtBackReturnsTrueWithoutMove) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  // The manager starts with one stream. It is the last stream, and it is idle.
  auto initial_last = mgr.GetLastStream();
  bool reused = mgr.ReuseIdleStreamToBack(
      [](Manager::Stream const& s) {
        return s.active_ranges.empty();
      });
  EXPECT_TRUE(reused);
  // Pointer should remain the same (it was already at the back)
  EXPECT_EQ(mgr.GetLastStream(), initial_last);
}

TEST(MultiStreamManagerTest, ReuseIdleStreamDoesNotMoveWhenWritePending) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  // Mark factory stream as not reusable
  mgr.GetLastStream()->stream->write_pending = true;
  auto s1 = std::make_shared<FakeStream>();
  s1->write_pending = true;  // also mark appended stream as not reusable
  mgr.AddStream(s1);
  bool moved = mgr.ReuseIdleStreamToBack(
      [](Manager::Stream const& s) {
        auto fs = s.stream.get();
        return fs != nullptr && s.active_ranges.empty() && !fs->write_pending;
      });
  EXPECT_FALSE(moved);
  auto it_last = mgr.GetLastStream();
  EXPECT_EQ(it_last->stream.get(), s1.get());
}

TEST(MultiStreamManagerTest, MoveActiveRangesTransfersAllEntries) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  auto s1 = std::make_shared<FakeStream>();
  auto s2 = std::make_shared<FakeStream>();
  auto it1 = mgr.AddStream(s1);
  auto it2 = mgr.AddStream(s2);
  it1->active_ranges.emplace(101, std::make_shared<FakeRange>());
  it1->active_ranges.emplace(202, std::make_shared<FakeRange>());
  ASSERT_EQ(it1->active_ranges.size(), 2u);
  ASSERT_TRUE(it2->active_ranges.empty());
  mgr.MoveActiveRanges(it1, it2);
  EXPECT_TRUE(it1->active_ranges.empty());
  EXPECT_EQ(it2->active_ranges.size(), 2u);
  EXPECT_TRUE(it2->active_ranges.count(101));
  EXPECT_TRUE(it2->active_ranges.count(202));
}

TEST(MultiStreamManagerTest, GetLastStreamReflectsRecentAppendAndReuse) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  auto s1 = std::make_shared<FakeStream>();
  mgr.AddStream(s1);
  EXPECT_EQ(mgr.GetLastStream()->stream.get(), s1.get());
  bool moved = mgr.ReuseIdleStreamToBack(
      [](Manager::Stream const& s) {
        return s.stream != nullptr && s.active_ranges.empty();
      });
  EXPECT_TRUE(moved);
  auto it_last = mgr.GetLastStream();
  EXPECT_NE(it_last->stream.get(), s1.get());
}

TEST(MultiStreamManagerTest, EmptyAndSizeTransitions) {
  auto mgr = MultiStreamManagerTest::MakeManager();
  EXPECT_FALSE(mgr.Empty());
  EXPECT_EQ(mgr.Size(), 1u);
  auto it = mgr.GetLastStream();
  mgr.RemoveStreamAndNotifyRanges(it, Status());
  EXPECT_TRUE(mgr.Empty());
  EXPECT_EQ(mgr.Size(), 0u);
  auto s = std::make_shared<FakeStream>();
  mgr.AddStream(s);
  EXPECT_FALSE(mgr.Empty());
  EXPECT_EQ(mgr.Size(), 1u);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
