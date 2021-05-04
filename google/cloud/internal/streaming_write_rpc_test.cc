// Copyright 2021 Google LLC
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

#include "google/cloud/internal/streaming_write_rpc.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string value;
};

using MockReturnType =
    std::unique_ptr<grpc::ClientReaderInterface<FakeResponse>>;

class MockWriter : public grpc::ClientWriterInterface<FakeRequest> {
 public:
  MOCK_METHOD(bool, Write, (FakeRequest const&, grpc::WriteOptions),
              (override));
  MOCK_METHOD(bool, WritesDone, (), (override));
  MOCK_METHOD(grpc::Status, Finish, (), (override));
};

TEST(StreamingWriteRpcImpl, SuccessfulStream) {
  auto mock = absl::make_unique<MockWriter>();
  auto response = absl::make_unique<FakeResponse>();
  auto* response_ptr = response.get();
  EXPECT_CALL(*mock, Write).Times(3).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, WritesDone).WillOnce(Return(true));
  EXPECT_CALL(*mock, Finish).WillOnce([response_ptr] {
    response_ptr->value = "on-finish";
    return grpc::Status::OK;
  });

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  for (std::string key : {"w0", "w1", "w2"}) {
    auto w = impl.Write(FakeRequest{key}, grpc::WriteOptions{});
    EXPECT_TRUE(absl::holds_alternative<absl::monostate>(w));
  }
  auto actual = impl.WritesDone();
  ASSERT_THAT(actual, IsOk());
  EXPECT_EQ("on-finish", actual->value);
}

TEST(StreamingWriteRpcImpl, ErrorInWrite) {
  auto mock = absl::make_unique<MockWriter>();
  auto response = absl::make_unique<FakeResponse>();
  EXPECT_CALL(*mock, Write)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return grpc::Status(grpc::StatusCode::ABORTED, "aborted");
  });

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  for (std::string key : {"w0", "w1"}) {
    auto w = impl.Write(FakeRequest{key}, grpc::WriteOptions{});
    EXPECT_TRUE(absl::holds_alternative<absl::monostate>(w));
  }
  auto w = impl.Write(FakeRequest{"w2"}, grpc::WriteOptions{});
  ASSERT_TRUE(absl::holds_alternative<Status>(w));
  EXPECT_THAT(absl::get<Status>(w), StatusIs(StatusCode::kAborted, "aborted"));
}

TEST(StreamingWriteRpcImpl, ErrorInWritesDone) {
  auto mock = absl::make_unique<MockWriter>();
  auto response = absl::make_unique<FakeResponse>();
  EXPECT_CALL(*mock, Write).WillOnce(Return(true)).WillOnce(Return(true));
  EXPECT_CALL(*mock, WritesDone).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce([] {
    return grpc::Status(grpc::StatusCode::ABORTED, "aborted");
  });

  StreamingWriteRpcImpl<FakeRequest, FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(response),
      std::move(mock));
  for (std::string key : {"w0", "w1"}) {
    auto w = impl.Write(FakeRequest{key}, grpc::WriteOptions{});
    EXPECT_TRUE(absl::holds_alternative<absl::monostate>(w));
  }
  auto result = impl.WritesDone();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted, "aborted"));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
