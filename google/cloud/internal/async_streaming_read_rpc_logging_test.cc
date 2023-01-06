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

#include "google/cloud/internal/async_streaming_read_rpc_logging.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/tracing_options.h"
#include "absl/memory/memory.h"
#include <google/protobuf/duration.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;

template <typename Response>
class MockAsyncStreamingReadRpc : public AsyncStreamingReadRpc<Response> {
 public:
  ~MockAsyncStreamingReadRpc() override = default;

  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<Response>>, Read, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
  MOCK_METHOD(StreamingRpcMetadata, GetRequestMetadata, (), (const, override));
};

class StreamingReadRpcLoggingTest : public ::testing::Test {
 protected:
};

using MockStream = MockAsyncStreamingReadRpc<google::protobuf::Duration>;
using TestedStream = AsyncStreamingReadRpcLogging<google::protobuf::Duration>;

TEST(StreamingReadRpcLoggingTest, Cancel) {
  ScopedLog log;

  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel()).Times(1);
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  stream.Cancel();
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Cancel"), HasSubstr("test-id"))));
}

TEST(StreamingReadRpcLoggingTest, Start) {
  ScopedLog log;

  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_TRUE(stream.Start().get());
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Start"), HasSubstr("test-id"))));
}

TEST(StreamingReadRpcLoggingTest, ReadWithValue) {
  ScopedLog log;

  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::make_optional(google::protobuf::Duration{}));
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_TRUE(stream.Read().get().has_value());
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Read"), HasSubstr("test-id"),
                             HasSubstr("Duration"))));
}

TEST(StreamingReadRpcLoggingTest, ReadWithoutValue) {
  ScopedLog log;

  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::optional<google::protobuf::Duration>{});
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  ASSERT_FALSE(stream.Read().get().has_value());
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Read"), HasSubstr("test-id"),
                             HasSubstr("[not-set]"))));
}

TEST(StreamingReadRpcLoggingTest, Finish) {
  ScopedLog log;

  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{StatusCode::kUnavailable, "try-again"});
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_THAT(stream.Finish().get(),
              StatusIs(StatusCode::kUnavailable, "try-again"));
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("Finish"), HasSubstr("test-id"),
                             HasSubstr("try-again"))));
}

TEST(StreamingReadRpcLoggingTest, GetRequestMetadata) {
  ScopedLog log;

  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, GetRequestMetadata).WillOnce([] {
    return StreamingRpcMetadata({{":test-only", "value"}});
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_THAT(stream.GetRequestMetadata(),
              Contains(Pair(":test-only", "value")));
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("GetRequestMetadata(test-id)"),
                             HasSubstr("{:test-only: value}"))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
