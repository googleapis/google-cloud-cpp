// Copyright 2021 Google LLC
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

#include "google/cloud/internal/streaming_read_rpc_logging.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/tracing_options.h"
#include "absl/types/variant.h"
#include <google/protobuf/duration.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;

template <typename ResponseType>
class MockStreamingReadRpc : public StreamingReadRpc<ResponseType> {
 public:
  ~MockStreamingReadRpc() override = default;
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::variant<Status, ResponseType>), Read, (), (override));
  MOCK_METHOD(RpcMetadata, GetRequestMetadata, (), (const, override));
};

class StreamingReadRpcLoggingTest : public ::testing::Test {
 protected:
  testing_util::ScopedLog log_;
};

TEST_F(StreamingReadRpcLoggingTest, Cancel) {
  auto mock =
      std::make_unique<MockStreamingReadRpc<google::protobuf::Duration>>();
  EXPECT_CALL(*mock, Cancel()).Times(1);
  StreamingReadRpcLogging<google::protobuf::Duration> reader(
      std::move(mock), TracingOptions{},
      google::cloud::internal::RequestIdForLogging());
  reader.Cancel();
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("Cancel")));
}

TEST_F(StreamingReadRpcLoggingTest, Read) {
  struct ResultVisitor {
    void operator()(Status const& status) {
      EXPECT_EQ(status.code(), StatusCode::kInvalidArgument);
      EXPECT_EQ(status.message(), "Invalid argument.");
    }
    void operator()(google::protobuf::Duration const& response) {
      EXPECT_EQ(response.seconds(), 42);
    }
  };

  auto mock =
      std::make_unique<MockStreamingReadRpc<google::protobuf::Duration>>();
  EXPECT_CALL(*mock, Read())
      .WillOnce([] {
        google::protobuf::Duration result;
        result.set_seconds(42);
        return result;
      })
      .WillOnce([] {
        Status status(StatusCode::kInvalidArgument, "Invalid argument.");
        return status;
      });
  StreamingReadRpcLogging<google::protobuf::Duration> reader(
      std::move(mock), TracingOptions{},
      google::cloud::internal::RequestIdForLogging());
  auto result = reader.Read();
  absl::visit(ResultVisitor(), result);
  auto log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Read")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("42s")));

  result = reader.Read();
  absl::visit(ResultVisitor(), result);
  log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Read")));
  EXPECT_THAT(
      log_lines,
      Contains(HasSubstr(StatusCodeToString(StatusCode::kInvalidArgument))));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Invalid argument.")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
