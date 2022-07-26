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

#include "google/cloud/internal/streaming_write_rpc_logging.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/tracing_options.h"
#include "absl/memory/memory.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Return;

template <typename RequestType, typename ResponseType>
class MockStreamingWriteRpc
    : public StreamingWriteRpc<RequestType, ResponseType> {
 public:
  ~MockStreamingWriteRpc() override = default;
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(bool, Write, (RequestType const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(StatusOr<ResponseType>, Close, (), (override));
  MOCK_METHOD(StreamingRpcMetadata, GetRequestMetadata, (), (const, override));
};

class StreamingWriteRpcLoggingTest : public ::testing::Test {
 protected:
  testing_util::ScopedLog log_;
};

using MockStream = MockStreamingWriteRpc<google::protobuf::Timestamp,
                                         google::protobuf::Duration>;
using TestedStream = StreamingWriteRpcLogging<google::protobuf::Timestamp,
                                              google::protobuf::Duration>;

TEST_F(StreamingWriteRpcLoggingTest, Cancel) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Cancel).Times(1);
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  stream.Cancel();
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("Cancel"), HasSubstr("test-id"))));
}

TEST_F(StreamingWriteRpcLoggingTest, Write) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Write).WillOnce(Return(true));
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  google::protobuf::Timestamp request;
  request.set_seconds(123456);
  stream.Write(request, grpc::WriteOptions{});
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Write"), HasSubstr("test-id"),
                                    HasSubstr("1970-01-02T10:17:36Z"))));
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Write"), HasSubstr("test-id"),
                                    HasSubstr("true"))));
}

TEST_F(StreamingWriteRpcLoggingTest, CloseWithSuccess) {
  auto mock = absl::make_unique<MockStream>();
  google::protobuf::Duration d;
  d.set_seconds(123456);
  EXPECT_CALL(*mock, Close).WillOnce(Return(make_status_or(d)));
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  auto response = stream.Close();
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(d));
  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Close"), HasSubstr("test-id"),
                                    HasSubstr("(void)"))));
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Close"), HasSubstr("test-id"),
                                    HasSubstr("34h17m36s"))));
}

TEST_F(StreamingWriteRpcLoggingTest, CloseWithError) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, Close)
      .WillOnce(Return(StatusOr<google::protobuf::Duration>(
          Status{StatusCode::kUnavailable, "try-again"})));
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  auto response = stream.Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable, "try-again"));

  auto const lines = log_.ExtractLines();
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Close"), HasSubstr("test-id"),
                                    HasSubstr("(void)"))));
  EXPECT_THAT(lines, Contains(AllOf(HasSubstr("Close"), HasSubstr("test-id"),
                                    HasSubstr("try-again"))));
}

TEST_F(StreamingWriteRpcLoggingTest, GetRequestMetadata) {
  auto mock = absl::make_unique<MockStream>();
  EXPECT_CALL(*mock, GetRequestMetadata).WillOnce([] {
    return StreamingRpcMetadata({{":test-only", "value"}});
  });
  TestedStream stream(std::move(mock), TracingOptions{}, "test-id");
  EXPECT_THAT(stream.GetRequestMetadata(),
              Contains(Pair(":test-only", "value")));
  EXPECT_THAT(log_.ExtractLines(),
              Contains(AllOf(HasSubstr("GetRequestMetadata(test-id)"),
                             HasSubstr("{:test-only: value}"))));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
