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
#include "generator/integration_tests/golden/iam_credentials_client.gcpcxx.pb.h"
#include "generator/integration_tests/golden/mocks/mock_iam_credentials_connection.gcpcxx.pb.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <google/iam/v1/policy.pb.h>
#include <google/protobuf/util/field_mask_util.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAreArray;

TEST(IAMCredentialsClientTest, CopyMoveEquality) {
  auto conn1 = std::make_shared<golden_mocks::MockIAMCredentialsConnection>();
  auto conn2 = std::make_shared<golden_mocks::MockIAMCredentialsConnection>();

  IAMCredentialsClient c1(conn1);
  IAMCredentialsClient c2(conn2);
  EXPECT_NE(c1, c2);

  // Copy construction
  IAMCredentialsClient c3 = c1;
  EXPECT_EQ(c3, c1);
  EXPECT_NE(c3, c2);

  // Copy assignment
  c3 = c2;
  EXPECT_EQ(c3, c2);

  // Move construction
  IAMCredentialsClient c4 = std::move(c3);
  EXPECT_EQ(c4, c2);

  // Move assignment
  c1 = std::move(c4);
  EXPECT_EQ(c1, c2);
}

TEST(IAMCredentialsClientTest, GenerateAccessToken) {
  auto mock = std::make_shared<golden_mocks::MockIAMCredentialsConnection>();
  std::string expected_name = "/projects/-/serviceAccounts/foo@bar.com";
  std::vector<std::string> expected_delegates = {"Tom", "Dick", "Harry"};
  std::vector<std::string> expected_scope = {"admin"};
  google::protobuf::Duration expected_lifetime;
  expected_lifetime.set_seconds(4321);
  EXPECT_CALL(*mock, GenerateAccessToken)
      .Times(2)
      .WillRepeatedly([expected_name, expected_delegates, expected_scope,
                       expected_lifetime](
                          ::google::test::admin::database::v1::
                              GenerateAccessTokenRequest const &request) {
        EXPECT_EQ(request.name(), expected_name);
        EXPECT_THAT(request.delegates(), ElementsAreArray(expected_delegates));
        EXPECT_THAT(request.scope(), ElementsAreArray(expected_scope));
        EXPECT_THAT(request.lifetime(), IsProtoEqual(expected_lifetime));
        ::google::test::admin::database::v1::GenerateAccessTokenResponse
            response;
        return response;
      });
  IAMCredentialsClient client(std::move(mock));
  auto response = client.GenerateAccessToken(expected_name, expected_delegates,
                                             expected_scope, expected_lifetime);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  request.set_name(expected_name);
  *request.mutable_delegates() = {expected_delegates.begin(),
                                  expected_delegates.end()};
  *request.add_scope() = "admin";
  *request.mutable_lifetime() = expected_lifetime;
  response = client.GenerateAccessToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(IAMCredentialsClientTest, GenerateIdToken) {
  auto mock = std::make_shared<golden_mocks::MockIAMCredentialsConnection>();
  std::string expected_name = "/projects/-/serviceAccounts/foo@bar.com";
  std::vector<std::string> expected_delegates = {"Tom", "Dick", "Harry"};
  std::string expected_audience = "Everyone";
  bool expected_include_email = true;
  EXPECT_CALL(*mock, GenerateIdToken)
      .Times(2)
      .WillRepeatedly(
          [expected_name, expected_delegates, expected_audience,
           expected_include_email](
              ::google::test::admin::database::v1::GenerateIdTokenRequest const
                  &request) {
            EXPECT_EQ(request.name(), expected_name);
            EXPECT_THAT(request.delegates(),
                        testing::ElementsAreArray(expected_delegates));
            EXPECT_EQ(request.audience(), expected_audience);
            EXPECT_EQ(request.include_email(), expected_include_email);
            ::google::test::admin::database::v1::GenerateIdTokenResponse
                response;
            return response;
          });
  IAMCredentialsClient client(std::move(mock));
  auto response =
      client.GenerateIdToken(expected_name, expected_delegates,
                             expected_audience, expected_include_email);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  request.set_name(expected_name);
  *request.mutable_delegates() = {expected_delegates.begin(),
                                  expected_delegates.end()};
  request.set_audience(expected_audience);
  request.set_include_email(expected_include_email);
  response = client.GenerateIdToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(IAMCredentialsClientTest, WriteLogEntries) {
  auto mock = std::make_shared<golden_mocks::MockIAMCredentialsConnection>();
  std::string expected_log_name = "projects/my_project/logs/my_log";
  std::map<std::string, std::string> expected_labels = {
      {"key1", "Tom"}, {"key2", "Dick"}, {"key3", "Harry"}};
  EXPECT_CALL(*mock, WriteLogEntries)
      .Times(2)
      .WillRepeatedly([expected_log_name, expected_labels](
                          ::google::test::admin::database::v1::
                              WriteLogEntriesRequest const &request) {
        EXPECT_EQ(request.log_name(), expected_log_name);
        std::map<std::string, std::string> labels = {request.labels().begin(),
                                                     request.labels().end()};
        EXPECT_THAT(labels,
                    testing::UnorderedElementsAreArray(expected_labels));
        ::google::test::admin::database::v1::WriteLogEntriesResponse response;
        return response;
      });
  IAMCredentialsClient client(std::move(mock));
  auto response = client.WriteLogEntries(expected_log_name, expected_labels);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  request.set_log_name(expected_log_name);
  *request.mutable_labels() = {expected_labels.begin(), expected_labels.end()};
  response = client.WriteLogEntries(request);
  EXPECT_STATUS_OK(response);
}

TEST(IAMCredentialsClientTest, ListLogs) {
  auto mock = std::make_shared<golden_mocks::MockIAMCredentialsConnection>();
  std::string expected_parent = "projects/my-project";
  EXPECT_CALL(*mock, ListLogs)
      .Times(2)
      .WillRepeatedly([expected_parent](::google::test::admin::database::v1::
                                            ListLogsRequest const &request) {
        EXPECT_EQ(request.parent(), expected_parent);
        return google::cloud::internal::MakePaginationRange<ListLogsRange>(
            ::google::test::admin::database::v1::ListLogsRequest{},
            [](::google::test::admin::database::v1::ListLogsRequest const &) {
              return StatusOr<
                  ::google::test::admin::database::v1::ListLogsResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](::google::test::admin::database::v1::ListLogsResponse const &) {
              return std::vector<std::string>{};
            });
      });
  IAMCredentialsClient client(std::move(mock));
  auto range = client.ListLogs(expected_parent);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent(expected_parent);
  range = client.ListLogs(request);
  begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

} // namespace
} // namespace golden
} // namespace GOOGLE_CLOUD_CPP_NS
} // namespace cloud
} // namespace google
