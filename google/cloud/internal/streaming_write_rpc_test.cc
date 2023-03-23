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

#include "google/cloud/internal/streaming_write_rpc_impl.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string value;
};

class MockWriter : public grpc::ClientWriterInterface<FakeRequest> {
 public:
  MOCK_METHOD(bool, Write, (FakeRequest const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(bool, WritesDone, (), (override));
  MOCK_METHOD(grpc::Status, Finish, (), (override));
};

TEST(StreamingWriteRpcImpl, SuccessfulStream) {
  auto mock = std::make_unique<MockWriter>();
  auto response = std::make_unique<FakeResponse>();
  auto* response_ptr = response.get();
  EXPECT_CALL(*mock, Write).Times(3).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, WritesDone).WillOnce(Return(true));
  EXPECT_CALL(*mock, Finish).WillOnce([response_ptr] {
    response_ptr->value = "on-finish";
    return grpc::Status::OK;
  });

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  for (std::string key : {"w0", "w1", "w2"}) {
    EXPECT_TRUE(impl.Write(FakeRequest{key}, grpc::WriteOptions{}));
  }
  auto actual = impl.Close();
  ASSERT_THAT(actual, IsOk());
  EXPECT_EQ("on-finish", actual->value);
}

TEST(StreamingWriteRpcImpl, ErrorInWrite) {
  auto mock = std::make_unique<MockWriter>();
  auto response = std::make_unique<FakeResponse>();
  EXPECT_CALL(*mock, Write)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, WritesDone).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return grpc::Status(grpc::StatusCode::ABORTED, "aborted");
  });

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  for (std::string key : {"w0", "w1"}) {
    EXPECT_TRUE(impl.Write(FakeRequest{key}, grpc::WriteOptions{}));
  }
  EXPECT_FALSE(impl.Write(FakeRequest{"w2"}, grpc::WriteOptions{}));
  EXPECT_THAT(impl.Close(), StatusIs(StatusCode::kAborted, "aborted"));
}

TEST(StreamingWriteRpcImpl, ErrorInWritesDone) {
  auto mock = std::make_unique<MockWriter>();
  auto response = std::make_unique<FakeResponse>();
  EXPECT_CALL(*mock, Write).WillOnce(Return(true)).WillOnce(Return(true));
  EXPECT_CALL(*mock, WritesDone).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return grpc::Status(grpc::StatusCode::ABORTED, "aborted");
  });

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  for (std::string key : {"w0", "w1"}) {
    EXPECT_TRUE(impl.Write(FakeRequest{key}, grpc::WriteOptions{}));
  }
  EXPECT_THAT(impl.Close(), StatusIs(StatusCode::kAborted, "aborted"));
}

TEST(StreamingWriteRpcImpl, NoWritesDoneWithLastMessage) {
  auto mock = std::make_unique<MockWriter>();
  auto response = std::make_unique<FakeResponse>();
  EXPECT_CALL(*mock, Write).WillOnce(Return(true)).WillOnce(Return(true));
  EXPECT_CALL(*mock, Finish).WillOnce(Return(grpc::Status::OK));

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  EXPECT_TRUE(impl.Write(FakeRequest{"w0"}, grpc::WriteOptions{}));
  EXPECT_TRUE(
      impl.Write(FakeRequest{"w1"}, grpc::WriteOptions{}.set_last_message()));
  EXPECT_THAT(impl.Close(), IsOk());
}

TEST(StreamingWriteRpcImpl, UnreportedErrors) {
  testing_util::ScopedLog log;

  StreamingWriteRpcReportUnhandledError(
      Status(StatusCode::kPermissionDenied, "uh-oh"),
      typeid(FakeRequest).name());
  auto lines = log.ExtractLines();
  EXPECT_THAT(
      lines, Contains(AllOf(HasSubstr("unhandled error"), HasSubstr("uh-oh"))));

  StreamingWriteRpcReportUnhandledError(Status(), typeid(FakeRequest).name());
  lines = log.ExtractLines();
  EXPECT_THAT(lines, IsEmpty());

  StreamingWriteRpcReportUnhandledError(
      Status(StatusCode::kCancelled, "CANCELLED"), typeid(FakeRequest).name());
  lines = log.ExtractLines();
  EXPECT_THAT(lines, IsEmpty());
}

TEST(StreamingWriteRpcError, ErrorStream) {
  auto under_test = StreamingWriteRpcError<FakeRequest, FakeResponse>(
      Status(StatusCode::kAborted, "aborted"));
  under_test.Cancel();  // just a smoke test
  EXPECT_FALSE(under_test.Write(FakeRequest{"w0"}, grpc::WriteOptions()));
  EXPECT_THAT(under_test.Close(), StatusIs(StatusCode::kAborted, "aborted"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
