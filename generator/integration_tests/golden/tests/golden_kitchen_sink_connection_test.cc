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

#include "generator/integration_tests/golden/golden_kitchen_sink_connection.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/golden/golden_kitchen_sink_options.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::Return;

class MockGoldenKitchenSinkStub
    : public google::cloud::golden_internal::GoldenKitchenSinkStub {
 public:
  ~MockGoldenKitchenSinkStub() override = default;
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListLogsResponse>, ListLogs,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListLogsRequest const& request),
      (override));
  MOCK_METHOD(
      (std::unique_ptr<internal::StreamingReadRpc<
           ::google::test::admin::database::v1::TailLogEntriesResponse>>),
      TailLogEntries,
      (std::unique_ptr<grpc::ClientContext> context,
       ::google::test::admin::database::v1::TailLogEntriesRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>,
      ListServiceAccountKeys,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListServiceAccountKeysRequest const&
           request),
      (override));
};

std::shared_ptr<golden::GoldenKitchenSinkConnection> CreateTestingConnection(
    std::shared_ptr<golden_internal::GoldenKitchenSinkStub> mock) {
  golden::GoldenKitchenSinkLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  GenericPollingPolicy<golden::GoldenKitchenSinkLimitedErrorCountRetryPolicy,
                       ExponentialBackoffPolicy>
      polling(retry, backoff);
  Options options;
  options.set<golden::GoldenKitchenSinkRetryPolicyOption>(retry.clone());
  options.set<golden::GoldenKitchenSinkBackoffPolicyOption>(backoff.clone());
  return golden::MakeGoldenKitchenSinkConnection(std::move(mock),
                                                 std::move(options));
}

TEST(GoldenKitchenSinkConnectionTest, GenerateAccessTokenSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce([](grpc::ClientContext&,
                   ::google::test::admin::database::v1::
                       GenerateAccessTokenRequest const&) {
        ::google::test::admin::database::v1::GenerateAccessTokenResponse
            response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto response = conn->GenerateAccessToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, GenerateAccessTokenPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto response = conn->GenerateAccessToken(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(GoldenKitchenSinkConnectionTest, GenerateAccessTokenTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto response = conn->GenerateAccessToken(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, GenerateIdTokenSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce([](grpc::ClientContext&, ::google::test::admin::database::v1::
                                             GenerateIdTokenRequest const&) {
        ::google::test::admin::database::v1::GenerateIdTokenResponse response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto response = conn->GenerateIdToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, GenerateIdTokenPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto response = conn->GenerateIdToken(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(GoldenKitchenSinkConnectionTest, GenerateIdTokenTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto response = conn->GenerateIdToken(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, WriteLogEntriesSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .WillOnce([](grpc::ClientContext&, ::google::test::admin::database::v1::
                                             WriteLogEntriesRequest const&) {
        ::google::test::admin::database::v1::WriteLogEntriesResponse response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto response = conn->WriteLogEntries(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, WriteLogEntriesPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto response = conn->WriteLogEntries(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(GoldenKitchenSinkConnectionTest, WriteLogEntriesTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto response = conn->WriteLogEntries(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListLogsSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  std::string const expected_parent = "projects/my-project";
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce([&expected_parent](
                    grpc::ClientContext&,
                    ::google::test::admin::database::v1::ListLogsRequest const&
                        request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());
        ::google::test::admin::database::v1::ListLogsResponse page;
        page.set_next_page_token("page-1");
        *page.add_log_names() = "log1";
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    grpc::ClientContext&,
                    ::google::test::admin::database::v1::ListLogsRequest const&
                        request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-1", request.page_token());
        ::google::test::admin::database::v1::ListLogsResponse page;
        page.set_next_page_token("page-2");
        *page.add_log_names() = "log2";
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    grpc::ClientContext&,
                    ::google::test::admin::database::v1::ListLogsRequest const&
                        request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-2", request.page_token());
        ::google::test::admin::database::v1::ListLogsResponse page;
        page.clear_next_page_token();
        *page.add_log_names() = "log3";
        return make_status_or(page);
      });
  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<std::string> actual_log_names;
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my-project");
  for (auto const& log_name : conn->ListLogs(request)) {
    ASSERT_STATUS_OK(log_name);
    actual_log_names.push_back(*log_name);
  }
  EXPECT_THAT(actual_log_names, ElementsAre("log1", "log2", "log3"));
}

TEST(GoldenKitchenSinkConnectionTest, ListLogsPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my-project");
  auto range = conn->ListLogs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListLogsTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListLogs)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my-project");
  auto range = conn->ListLogs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kUnavailable, begin->status().code());
}

class MockTailLogEntriesStreamingReadRpc
    : public internal::StreamingReadRpc<
          google::test::admin::database::v1::TailLogEntriesResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(
      (absl::variant<
          Status, google::test::admin::database::v1::TailLogEntriesResponse>),
      Read, (), (override));
};

std::unique_ptr<MockTailLogEntriesStreamingReadRpc> MakeFailingReader(
    Status status) {
  auto reader = absl::make_unique<MockTailLogEntriesStreamingReadRpc>();
  EXPECT_CALL(*reader, Read).WillOnce(Return(status));
  return reader;
}

TEST(GoldenKitchenSinkConnectionTest, TailLogEntriesPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, TailLogEntries)
      .WillOnce(Return(ByMove(MakeFailingReader(
          Status(StatusCode::kPermissionDenied, "Permission Denied.")))));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::TailLogEntriesRequest request;
  auto range = conn->TailLogEntries(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListServiceAccountKeysSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([](grpc::ClientContext&,
                   ::google::test::admin::database::v1::
                       ListServiceAccountKeysRequest const&) {
        ::google::test::admin::database::v1::ListServiceAccountKeysResponse
            response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  auto response = conn->ListServiceAccountKeys(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, ListServiceAccountKeysTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  auto response = conn->ListServiceAccountKeys(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListServiceAccountKeysPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  auto response = conn->ListServiceAccountKeys(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden
}  // namespace cloud
}  // namespace google
