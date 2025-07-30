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

#include "google/cloud/internal/streaming_read_rpc.h"
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
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string value;
};

class MockReader : public grpc::ClientReaderInterface<FakeResponse> {
 public:
  MOCK_METHOD(bool, Read, (FakeResponse*), (override));
  MOCK_METHOD(grpc::Status, Finish, (), (override));

  // Unused. We define them (as they are pure virtual), but not as mocks.
  bool NextMessageSize(std::uint32_t*) override { return true; }
  void WaitForInitialMetadata() override {}
};

TEST(StreamingReadRpcImpl, SuccessfulStream) {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](FakeResponse* r) {
        r->value = "value-0";
        return true;
      })
      .WillOnce([](FakeResponse* r) {
        r->value = "value-1";
        return true;
      })
      .WillOnce([](FakeResponse* r) {
        r->value = "value-2";
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce(Return(grpc::Status::OK));

  StreamingReadRpcImpl<FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(mock));
  std::vector<std::string> values;
  for (;;) {
    FakeResponse response;
    auto status = impl.Read(&response);
    if (status.has_value()) {
      EXPECT_THAT(*status, IsOk());
      break;
    }
    values.push_back(response.value);
  }
  EXPECT_THAT(values, ElementsAre("value-0", "value-1", "value-2"));
}

TEST(StreamingReadRpcImpl, EmptyStream) {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce(Return(grpc::Status::OK));

  StreamingReadRpcImpl<FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(mock));
  FakeResponse response;
  auto status = impl.Read(&response);
  ASSERT_TRUE(status.has_value());
  EXPECT_THAT(*status, IsOk());
}

TEST(StreamingReadRpcImpl, EmptyWithError) {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  StreamingReadRpcImpl<FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(mock));
  FakeResponse response;
  auto status = impl.Read(&response);
  ASSERT_TRUE(status.has_value());
  EXPECT_THAT(*status, StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(StreamingReadRpcImpl, ErrorAfterData) {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](FakeResponse* r) {
        r->value = "test-value-0";
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  StreamingReadRpcImpl<FakeResponse> impl(
      std::make_shared<grpc::ClientContext>(), std::move(mock));
  std::vector<std::string> values;
  for (;;) {
    FakeResponse response;
    auto status = impl.Read(&response);
    if (status.has_value()) {
      EXPECT_THAT(*status, StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
      break;
    }
    values.push_back(response.value);
  }
  EXPECT_THAT(values, ElementsAre("test-value-0"));
}

TEST(StreamingReadRpcImpl, HandleUnfinished) {
  auto mock = std::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](FakeResponse* r) {
        r->value = "value-0";
        return true;
      })
      .WillOnce([](FakeResponse* r) {
        r->value = "value-1";
        return true;
      });
  EXPECT_CALL(*mock, Finish)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  testing_util::ScopedLog log;

  {
    StreamingReadRpcImpl<FakeResponse> impl(
        std::make_shared<grpc::ClientContext>(), std::move(mock));
    std::vector<std::string> values;
    FakeResponse response;
    EXPECT_FALSE(impl.Read(&response).has_value());
    values.push_back(response.value);
    EXPECT_FALSE(impl.Read(&response).has_value());
    values.push_back(response.value);
    EXPECT_THAT(values, ElementsAre("value-0", "value-1"));
  }
  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("unhandled error"), HasSubstr("status="),
                             HasSubstr("uh-oh"))));
}

TEST(StreamingReadRpcImpl, HandleUnfinishedExpected) {
  for (auto const& status : {grpc::Status::OK, grpc::Status::CANCELLED}) {
    auto mock = std::make_unique<MockReader>();
    EXPECT_CALL(*mock, Read)
        .WillOnce([](FakeResponse* r) {
          r->value = "value-0";
          return true;
        })
        .WillOnce([](FakeResponse* r) {
          r->value = "value-1";
          return true;
        });
    EXPECT_CALL(*mock, Finish).WillOnce(Return(status));

    testing_util::ScopedLog log;
    {
      StreamingReadRpcImpl<FakeResponse> impl(
          std::make_shared<grpc::ClientContext>(), std::move(mock));
      std::vector<std::string> values;
      FakeResponse response;
      EXPECT_FALSE(impl.Read(&response).has_value());
      values.push_back(response.value);
      EXPECT_FALSE(impl.Read(&response).has_value());
      values.push_back(response.value);
      EXPECT_THAT(values, ElementsAre("value-0", "value-1"));
    }
    EXPECT_THAT(log.ExtractLines(),
                Not(Contains(HasSubstr("unhandled error"))));
  }
}

TEST(StreamingReadRpcImpl, ErrorStream) {
  auto under_test = StreamingReadRpcError<FakeResponse>(
      Status(StatusCode::kPermissionDenied, "uh-oh"));
  under_test.Cancel();  // just a smoke test
  FakeResponse response;
  auto status = under_test.Read(&response);
  ASSERT_TRUE(status.has_value());
  EXPECT_THAT(*status, StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
