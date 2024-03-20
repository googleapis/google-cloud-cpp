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

#include "google/cloud/internal/async_read_write_stream_timeout.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/async_sequencer.h"
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
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Optional;
using ::testing::Return;

using MockStream =
    google::cloud::mocks::MockAsyncStreamingReadWriteRpc<int, std::string>;
using TestedStream = AsyncStreamingReadWriteRpcTimeout<int, std::string>;

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
auto constexpr kWriteTimeout = std::chrono::milliseconds(3456);

TEST(AsyncStreamingReadWriteRpcTimeout, Cancel) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  uut.Cancel();
}

TEST(AsyncStreamingReadWriteRpcTimeout, StartSuccess) {
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

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Start();
  auto timer = sequencer.PopFrontWithName();
  auto start = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(start.second, "Start");
  start.first.set_value(true);
  timer.first.set_value(false);
  EXPECT_TRUE(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, StartTimeout) {
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

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Start();
  auto timer = sequencer.PopFrontWithName();
  auto start = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(start.second, "Start");
  timer.first.set_value(true);
  start.first.set_value(true);
  EXPECT_FALSE(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, ReadSuccess) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read").then(
        [](auto) { return absl::make_optional(std::string("42")); });
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kReadTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Read();
  auto timer = sequencer.PopFrontWithName();
  auto read = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(read.second, "Read");
  read.first.set_value(true);
  timer.first.set_value(false);
  EXPECT_THAT(result.get(), Optional(std::string("42")));
}

TEST(AsyncStreamingReadWriteRpcTimeout, ReadTimeout) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([&] {
    return sequencer.PushBack("Read").then(
        [](auto) { return absl::make_optional(std::string("42")); });
  });
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kReadTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Read();
  auto timer = sequencer.PopFrontWithName();
  auto read = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(read.second, "Read");
  timer.first.set_value(true);
  read.first.set_value(true);
  EXPECT_EQ(result.get(), absl::optional<std::string>{});
}

TEST(AsyncStreamingReadWriteRpcTimeout, WriteSuccess) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Write(42, _)).WillOnce([&] {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kWriteTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Write(42, grpc::WriteOptions{});
  auto timer = sequencer.PopFrontWithName();
  auto write = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(write.second, "Write");
  write.first.set_value(true);
  timer.first.set_value(false);
  EXPECT_TRUE(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, WriteTimeout) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Write(42, _)).WillOnce([&] {
    return sequencer.PushBack("Write");
  });
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kWriteTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Write(42, grpc::WriteOptions{});
  auto timer = sequencer.PopFrontWithName();
  auto write = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(write.second, "Write");
  timer.first.set_value(true);
  write.first.set_value(true);
  EXPECT_FALSE(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, WritesDoneSuccess) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, WritesDone).WillOnce([&] {
    return sequencer.PushBack("WritesDone");
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kWriteTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.WritesDone();
  auto timer = sequencer.PopFrontWithName();
  auto done = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(done.second, "WritesDone");
  done.first.set_value(true);
  timer.first.set_value(false);
  EXPECT_TRUE(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, WritesDoneTimeout) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, WritesDone).WillOnce([&] {
    return sequencer.PushBack("WritesDone");
  });
  EXPECT_CALL(*mock, Cancel).Times(1);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq,
              MakeRelativeTimer(std::chrono::nanoseconds(kWriteTimeout)))
      .WillOnce([&](auto) {
        return sequencer.PushBack("MakeRelativeTimer").then(MakeTimerStatus);
      });

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.WritesDone();
  auto timer = sequencer.PopFrontWithName();
  auto done = sequencer.PopFrontWithName();
  ASSERT_THAT(timer.second, "MakeRelativeTimer");
  ASSERT_THAT(done.second, "WritesDone");
  timer.first.set_value(true);
  done.first.set_value(true);
  EXPECT_FALSE(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, Finish) {
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, Finish).WillOnce([&] {
    return sequencer.PushBack("Finish").then([](auto f) {
      if (f.get()) return Status{};
      return internal::DataLossError("uh oh", GCP_ERROR_INFO());
    });
  });
  EXPECT_CALL(*mock, Cancel).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto result = uut.Finish();
  auto finish = sequencer.PopFrontWithName();
  ASSERT_THAT(finish.second, "Finish");
  finish.first.set_value(true);
  EXPECT_STATUS_OK(result.get());
}

TEST(AsyncStreamingReadWriteRpcTimeout, GetRequestMetadata) {
  auto const expected =
      RpcMetadata{{{"k1", "v1"}, {"k2", "v2"}}, {{"t1", "tv1"}}};
  AsyncSequencer<bool> sequencer;
  auto mock = std::make_unique<MockStream>();
  EXPECT_CALL(*mock, GetRequestMetadata).WillOnce(Return(expected));

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);

  CompletionQueue cq(mock_cq);

  TestedStream uut(cq, kStartTimeout, kReadTimeout, kWriteTimeout,
                   std::move(mock));
  auto const actual = uut.GetRequestMetadata();
  EXPECT_THAT(actual.headers, ElementsAreArray(expected.headers));
  EXPECT_THAT(actual.trailers, ElementsAreArray(expected.trailers));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
