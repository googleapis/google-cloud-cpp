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

#include "generator/integration_tests/golden/iam_credentials_connection.gcpcxx.pb.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::Return;

class MockIAMCredentialsStub
    : public google::cloud::golden_internal::IAMCredentialsStub {
public:
  ~MockIAMCredentialsStub() override = default;
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const
           &request),
      (override));

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const
           &request),
      (override));
};

std::shared_ptr<golden::IAMCredentialsConnection> CreateTestingConnection(
    std::shared_ptr<golden_internal::IAMCredentialsStub> mock) {
  golden::IAMCredentialsLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  GenericPollingPolicy<golden::IAMCredentialsLimitedErrorCountRetryPolicy,
                       ExponentialBackoffPolicy>
      polling(retry, backoff);
  return golden::MakeIAMCredentialsConnection(
      std::move(mock), retry.clone(), backoff.clone(),
      golden::MakeDefaultIAMCredentialsConnectionIdempotencyPolicy());
}

TEST(IAMCredentialsConnectionTest, GenerateAccessTokenSuccess) {
  auto mock = std::make_shared<MockIAMCredentialsStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce([](grpc::ClientContext &,
                   ::google::test::admin::database::v1::
                       GenerateAccessTokenRequest const &) {
        ::google::test::admin::database::v1::GenerateAccessTokenResponse
            response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto response = conn->GenerateAccessToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(IAMCredentialsConnectionTest, GenerateAccessTokenPermanentError) {
  auto mock = std::make_shared<MockIAMCredentialsStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto response = conn->GenerateAccessToken(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(IAMCredentialsConnectionTest, GenerateAccessTokenTooManyTransients) {
  auto mock = std::make_shared<MockIAMCredentialsStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto response = conn->GenerateAccessToken(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(IAMCredentialsConnectionTest, GenerateIdTokenSuccess) {
  auto mock = std::make_shared<MockIAMCredentialsStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce([](grpc::ClientContext &,
                   ::google::test::admin::database::v1::
                       GenerateIdTokenRequest const &) {
        ::google::test::admin::database::v1::GenerateIdTokenResponse
            response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto response = conn->GenerateIdToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(IAMCredentialsConnectionTest, GenerateIdTokenPermanentError) {
  auto mock = std::make_shared<MockIAMCredentialsStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto response = conn->GenerateIdToken(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(IAMCredentialsConnectionTest, GenerateIdTokenTooManyTransients) {
  auto mock = std::make_shared<MockIAMCredentialsStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto response = conn->GenerateIdToken(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}


} // namespace
} // namespace golden_internal
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
