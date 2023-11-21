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

#include "google/cloud/internal/async_streaming_read_rpc_timeout.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::ElementsAreArray;
using ::testing::Return;

using MockStream = google::cloud::testing_util::MockAsyncStreamingReadRpc<int>;
using TestedStream = AsyncStreamingReadRpcTimeout<int>;

StatusOr<std::chrono::system_clock::time_point> CancelledTimer() {
  return internal::CancelledError("test-only", GCP_ERROR_INFO());
}

StatusOr<std::chrono::system_clock::time_point> MakeTimerStatus(
    future<bool> f) {
  if (!f.get()) return CancelledTimer();
  return make_status_or(std::chrono::system_clock::now());
}

auto constexpr kStartTimeout = std::chrono::milliseconds(1234);
auto constexpr kReadTimeout = std::chrono::milliseconds(2345);

TEST(AsyncStreamingReadRpcTimeout, Cancel) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  uut.Cancel();
}

TEST(AsyncStreamingReadRpcTimeout, StartSuccess) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kStartTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  auto result = uut.Start();
  auto timer = sequencer.PopFrontWithName();
  auto start = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(start.second, "Start");
  start.first.set_value(true);
  timer.first.set_value(false);
  EXPECT_TRUE(result.get());
}

TEST(AsyncStreamingReadRpcTimeout, StartTimeout) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Start).WillOnce([&] {
    return sequencer.PushBack("Start");
  });
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kStartTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  auto result = uut.Start();
  auto timer = sequencer.PopFrontWithName();
  auto start = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(start.second, "Start");
  timer.first.set_value(true);
  start.first.set_value(true);
  EXPECT_FALSE(result.get());
}

TEST(AsyncStreamingReadRpcTimeout, ReadSuccess) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<int>();
      return absl::make_optional(42);
    });
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kReadTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  auto result = uut.Read();
  auto timer = sequencer.PopFrontWithName();
  auto read = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(read.second, "Read");
  read.first.set_value(true);
  timer.first.set_value(false);
  EXPECT_EQ(result.get().value_or(-1), 42);
}

TEST(AsyncStreamingReadRpcTimeout, ReadTimeout) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read").then([](auto f) {
      if (!f.get()) return absl::optional<int>();
      return absl::make_optional(42);
    });
  });
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kReadTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  auto result = uut.Read();
  auto timer = sequencer.PopFrontWithName();
  auto read = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(read.second, "Read");
  timer.first.set_value(true);
  read.first.set_value(true);
  EXPECT_EQ(result.get().value_or(-1), -1);
}

TEST(AsyncStreamingReadRpcTimeout, Finish) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (!f.get()) return internal::DataLossError("uh oh", GCP_ERROR_INFO());
      return Status{};
    });
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  auto result = uut.Finish();
  auto finish = sequencer.PopFrontWithName();
  ASSERT_THAT(finish.second, "Finish");
  finish.first.set_value(true);
  ASSERT_STATUS_OK(result.get());
}

TEST(AsyncStreamingReadRpcTimeout, GetRequestMetadata) {
  auto const expected =
      RpcMetadata{{{"k1", "v1"}, {"k2", "v2"}}, {{"t1", "tv1"}}};
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, GetRequestMetadata).WillOnce(Return(expected));

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, std::move(mock));
  auto const actual = uut.GetRequestMetadata();
  EXPECT_THAT(actual.headers, ElementsAreArray(expected.headers));
  EXPECT_THAT(actual.trailers, ElementsAreArray(expected.trailers));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
