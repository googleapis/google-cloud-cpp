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

#include "google/cloud/internal/async_read_write_stream_logging.h"
#include "google/cloud/log.h"
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

using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;

template <typename Request, typename Response>
class MockAsyncStreamingReadWriteRpc
    : public AsyncStreamingReadWriteRpc<Request, Response> {
 public:
  ~MockAsyncStreamingReadWriteRpc() override = default;

  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<Response>>, Read, (), (override));
  MOCK_METHOD(future<bool>, Write, (Request const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
};

class StreamingReadRpcLoggingTest : public ::testing::Test {
 protected:
  testing_util::ScopedLog log_;
};

using MockStream = MockAsyncStreamingReadWriteRpc<google::protobuf::Duration,
                                                  google::protobuf::Duration>;
using TestedStream =
    AsyncStreamingReadWriteRpcLogging<google::protobuf::Duration,
                                      google::protobuf::Duration>;

TEST_F(StreamingReadRpcLoggingTest, Cancel) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel()).Times(1);
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  stream.Cancel();
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Cancel"), HasSubstr("test-id"))));
}

TEST_F(StreamingReadRpcLoggingTest, Start) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Start).WillOnce([] { return make_ready_future(true); });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_TRUE(stream.Start().get());
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Start"), HasSubstr("test-id"))));
}

TEST_F(StreamingReadRpcLoggingTest, ReadWithValue) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::make_optional(google::protobuf::Duration{}));
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_TRUE(stream.Read().get().has_value());
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Read"), HasSubstr("test-id"),
                             HasSubstr("Duration"))));
}

TEST_F(StreamingReadRpcLoggingTest, ReadWithout) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Read).WillOnce([] {
    return make_ready_future(absl::optional<google::protobuf::Duration>{});
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  ASSERT_FALSE(stream.Read().get().has_value());
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Read"), HasSubstr("test-id"),
                             HasSubstr("[not-set]"))));
}

TEST_F(StreamingReadRpcLoggingTest, Write) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Write)
      .WillOnce([](google::protobuf::Duration const&, grpc::WriteOptions) {
        return make_ready_future(true);
      });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_TRUE(
      stream.Write(google::protobuf::Duration{}, grpc::WriteOptions{}).get());
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Write"), HasSubstr("test-id"))));
}

TEST_F(StreamingReadRpcLoggingTest, WritesDone) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, WritesDone).WillOnce([] {
    return make_ready_future(true);
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_TRUE(stream.WritesDone().get());
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("WritesDone"), HasSubstr("test-id"))));
}

TEST_F(StreamingReadRpcLoggingTest, Finish) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return make_ready_future(Status{StatusCode::kUnavailable, "try-again"});
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_THAT(stream.Finish().get(),
              StatusIs(StatusCode::kUnavailable, "try-again"));
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Finish"), HasSubstr("test-id"),
                             HasSubstr("try-again"))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
