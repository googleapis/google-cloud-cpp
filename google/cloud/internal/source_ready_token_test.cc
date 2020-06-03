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

#include "google/cloud/internal/source_ready_token.h"
#include <gmock/gmock.h>
#include <future>
#include <thread>
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

TEST(ReadyTokenFlowControlTest, Basic) {
  auto constexpr kOutstanding = 3;
  ReadyTokenFlowControl tested(kOutstanding);
  std::vector<future<ReadyToken>> tokens;

  for (int i = 0; i != 2 * kOutstanding; ++i) {
    tokens.push_back(tested.Acquire());
  }

  for (int i = 0; i != kOutstanding; ++i) {
    SCOPED_TRACE("Testing for " + std::to_string(i));
    ASSERT_TRUE(tokens[i].is_ready());
    ASSERT_FALSE(tokens[kOutstanding + i].is_ready());
  }
  for (int i = 0; i != kOutstanding; ++i) {
    SCOPED_TRACE("Testing for " + std::to_string(i));
    EXPECT_TRUE(tested.Release(tokens[i].get()));
  }
  for (int i = 0; i != kOutstanding; ++i) {
    SCOPED_TRACE("Testing for " + std::to_string(i));
    ASSERT_TRUE(tokens[kOutstanding + i].is_ready());
    EXPECT_TRUE(tested.Release(tokens[kOutstanding + i].get()));
  }
}

TEST(ReadyTokenFlowControlTest, Assignment) {
  auto constexpr kOutstanding = 3;
  ReadyTokenFlowControl original(kOutstanding);
  EXPECT_EQ(original.max_outstanding(), kOutstanding);
  auto copy = std::move(original);
  EXPECT_EQ(copy.max_outstanding(), kOutstanding);
  original = ReadyTokenFlowControl(2 * kOutstanding);
  EXPECT_EQ(original.max_outstanding(), 2 * kOutstanding);
}

TEST(ReadyTokenFlowControlTest, TokenManip) {
  auto constexpr kOutstanding = 3;
  ReadyTokenFlowControl original(kOutstanding);

  auto token = original.Acquire().get();
  EXPECT_TRUE(token.valid());
  EXPECT_TRUE(static_cast<bool>(token));
  ReadyToken invalid;
  EXPECT_FALSE(invalid.valid());
  EXPECT_FALSE(static_cast<bool>(invalid));
  invalid = std::move(token);
  EXPECT_TRUE(invalid.valid());
  EXPECT_TRUE(static_cast<bool>(invalid));
  EXPECT_FALSE(token.valid());             // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(static_cast<bool>(token));  // NOLINT(bugprone-use-after-move)

  ReadyToken copy(std::move(invalid));
  EXPECT_TRUE(copy.valid());
  EXPECT_TRUE(static_cast<bool>(copy));

  original.Release(std::move(copy));
  EXPECT_FALSE(std::is_copy_assignable<ReadyToken>::value);
  EXPECT_TRUE(std::is_move_assignable<ReadyToken>::value);
  EXPECT_FALSE(std::is_copy_constructible<ReadyToken>::value);
  EXPECT_TRUE(std::is_move_constructible<ReadyToken>::value);
}

TEST(ReadyTokenFlowControlTest, Threaded) {
  auto constexpr kOutstanding = 16;
  auto constexpr kThreads = 64;
  auto constexpr kIterations = kThreads * 10000;

  ReadyTokenFlowControl tested(kOutstanding);
  std::atomic<int> outstanding(0);
  std::atomic<int> pending(kIterations);
  auto work = [&] {
    int observed_max = std::numeric_limits<int>::min();
    while (--pending > 0) {
      auto token = tested.Acquire().get();
      auto const pre_release = ++outstanding;
      observed_max = (std::max)(pre_release, observed_max);
      --outstanding;
      tested.Release(std::move(token));
    }
    return observed_max;
  };
  std::vector<std::future<int>> tasks;
  std::generate_n(std::back_inserter(tasks), kThreads,
                  [&] { return std::async(std::launch::async, work); });
  for (auto& t : tasks) {
    auto observed_max = t.get();
    EXPECT_LE(observed_max, kOutstanding);
  }
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
