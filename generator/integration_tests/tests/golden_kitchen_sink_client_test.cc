// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/v1/golden_kitchen_sink_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "generator/integration_tests/golden/v1/mocks/mock_golden_kitchen_sink_connection.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_stub.h"
#include <google/iam/v1/policy.pb.h>
#include <google/protobuf/util/field_mask_util.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::MockAsyncStreamingReadWriteRpc;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAreArray;

TEST(GoldenKitchenSinkClientTest, CopyMoveEquality) {
  auto conn1 =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  auto conn2 =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*conn1, options);
  EXPECT_CALL(*conn2, options);

  GoldenKitchenSinkClient c1(conn1);
  GoldenKitchenSinkClient c2(conn2);
  EXPECT_NE(c1, c2);

  // Copy construction
  GoldenKitchenSinkClient c3 = c1;
  EXPECT_EQ(c3, c1);
  EXPECT_NE(c3, c2);

  // Copy assignment
  c3 = c2;
  EXPECT_EQ(c3, c2);

  // Move construction
  GoldenKitchenSinkClient c4 = std::move(c3);
  EXPECT_EQ(c4, c2);

  // Move assignment
  c1 = std::move(c4);
  EXPECT_EQ(c1, c2);
}

TEST(GoldenKitchenSinkClientTest, GenerateAccessToken) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options);

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
                              GenerateAccessTokenRequest const& request) {
        EXPECT_EQ(request.name(), expected_name);
        EXPECT_THAT(request.delegates(), ElementsAreArray(expected_delegates));
        EXPECT_THAT(request.scope(), ElementsAreArray(expected_scope));
        EXPECT_THAT(request.lifetime(), IsProtoEqual(expected_lifetime));
        ::google::test::admin::database::v1::GenerateAccessTokenResponse
            response;
        return response;
      });
  GoldenKitchenSinkClient client(std::move(mock));
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

TEST(GoldenKitchenSinkClientTest, GenerateIdToken) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options);

  std::string expected_name = "/projects/-/serviceAccounts/foo@bar.com";
  std::vector<std::string> expected_delegates = {"Tom", "Dick", "Harry"};
  std::string expected_audience = "Everyone";
  bool expected_include_email = true;
  EXPECT_CALL(*mock, GenerateIdToken)
      .Times(2)
      .WillRepeatedly([expected_name, expected_delegates, expected_audience,
                       expected_include_email](
                          ::google::test::admin::database::v1::
                              GenerateIdTokenRequest const& request) {
        EXPECT_EQ(request.name(), expected_name);
        EXPECT_THAT(request.delegates(),
                    testing::ElementsAreArray(expected_delegates));
        EXPECT_EQ(request.audience(), expected_audience);
        EXPECT_EQ(request.include_email(), expected_include_email);
        ::google::test::admin::database::v1::GenerateIdTokenResponse response;
        return response;
      });
  GoldenKitchenSinkClient client(std::move(mock));
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

TEST(GoldenKitchenSinkClientTest, WriteLogEntries) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options);

  std::string expected_log_name = "projects/my_project/logs/my_log";
  std::map<std::string, std::string> expected_labels = {
      {"key1", "Tom"}, {"key2", "Dick"}, {"key3", "Harry"}};
  EXPECT_CALL(*mock, WriteLogEntries)
      .Times(2)
      .WillRepeatedly([expected_log_name, expected_labels](
                          ::google::test::admin::database::v1::
                              WriteLogEntriesRequest const& request) {
        EXPECT_EQ(request.log_name(), expected_log_name);
        std::map<std::string, std::string> labels = {request.labels().begin(),
                                                     request.labels().end()};
        EXPECT_THAT(labels, UnorderedElementsAreArray(expected_labels));
        ::google::test::admin::database::v1::WriteLogEntriesResponse response;
        return response;
      });
  GoldenKitchenSinkClient client(std::move(mock));
  auto response = client.WriteLogEntries(expected_log_name, expected_labels);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  request.set_log_name(expected_log_name);
  *request.mutable_labels() = {expected_labels.begin(), expected_labels.end()};
  response = client.WriteLogEntries(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkClientTest, ListLogs) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options);

  std::string expected_parent = "projects/my-project";
  EXPECT_CALL(*mock, ListLogs)
      .Times(2)
      .WillRepeatedly([expected_parent](::google::test::admin::database::v1::
                                            ListLogsRequest const& request) {
        EXPECT_EQ(request.parent(), expected_parent);
        return google::cloud::internal::MakePaginationRange<
            StreamRange<std::string>>(
            ::google::test::admin::database::v1::ListLogsRequest{},
            [](::google::test::admin::database::v1::ListLogsRequest const&) {
              return StatusOr<
                  ::google::test::admin::database::v1::ListLogsResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](::google::test::admin::database::v1::ListLogsResponse const&) {
              return std::vector<std::string>{};
            });
      });
  GoldenKitchenSinkClient client(std::move(mock));
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

TEST(GoldenKitchenSinkClientTest, StreamingRead) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly([] {
    return Options{}
        .set<GrpcTracingOptionsOption>(
            TracingOptions().SetOptions("truncate_string_field_longer_than=64"))
        .set<EndpointOption>("override-me")
        .set<UserAgentProductsOption>({"override-me"});
  });

  EXPECT_CALL(*mock, StreamingRead)
      .Times(2)
      .WillRepeatedly([](Request const& request) {
        auto const& current = internal::CurrentOptions();
        EXPECT_TRUE(current.has<EndpointOption>());
        EXPECT_TRUE(current.has<GrpcTracingOptionsOption>());
        EXPECT_TRUE(current.has<UserAgentProductsOption>());
        EXPECT_EQ(current.get<EndpointOption>(), "test-endpoint");
        EXPECT_EQ(current.get<GrpcTracingOptionsOption>()
                      .truncate_string_field_longer_than(),
                  64);
        EXPECT_THAT(current.get<UserAgentProductsOption>(),
                    Contains("test-only/1.0"));
        EXPECT_THAT(request.stream(), "test-only-stream-name");
        return google::cloud::internal::MakeStreamRange<Response>(
            []() -> absl::variant<Status, Response> {
              return Status(StatusCode::kPermissionDenied, "uh-oh");
            });
      });
  GoldenKitchenSinkClient client(
      std::move(mock), Options{}
                           .set<EndpointOption>("test-endpoint")
                           .set<UserAgentProductsOption>({"override-me-too"}));
  auto options = Options{}.set<UserAgentProductsOption>({"test-only/1.0"});
  auto range = client.StreamingRead("test-only-stream-name", options);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
  auto request = Request{};
  request.set_stream("test-only-stream-name");
  range = client.StreamingRead(request, options);
  begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkClientTest, ListServiceAccountKeys) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options);

  std::string expected_name =
      "/projects/my-project/serviceAccounts/foo@bar.com";
  std::vector<::google::test::admin::database::v1::
                  ListServiceAccountKeysRequest::KeyType>
      expected_key_types = {::google::test::admin::database::v1::
                                ListServiceAccountKeysRequest::SYSTEM_MANAGED};
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .Times(2)
      .WillRepeatedly([expected_name, expected_key_types](
                          ::google::test::admin::database::v1::
                              ListServiceAccountKeysRequest const& request) {
        EXPECT_EQ(request.name(), expected_name);
        EXPECT_THAT(request.key_types(),
                    testing::ElementsAreArray(expected_key_types));
        ::google::test::admin::database::v1::ListServiceAccountKeysResponse
            response;
        return response;
      });
  GoldenKitchenSinkClient client(std::move(mock));
  auto response =
      client.ListServiceAccountKeys(expected_name, expected_key_types);
  EXPECT_STATUS_OK(response);
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  request.set_name(expected_name);
  *request.mutable_key_types() = {expected_key_types.begin(),
                                  expected_key_types.end()};
  response = client.ListServiceAccountKeys(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkClientTest, AsyncStreamingReadWrite) {
  auto mock =
      std::make_shared<golden_v1_mocks::MockGoldenKitchenSinkConnection>();
  EXPECT_CALL(*mock, options).WillRepeatedly([] {
    return Options{}
        .set<GrpcTracingOptionsOption>(
            TracingOptions().SetOptions("truncate_string_field_longer_than=64"))
        .set<EndpointOption>("override-me")
        .set<UserAgentProductsOption>({"override-me"});
  });

  EXPECT_CALL(*mock, AsyncStreamingReadWrite).WillOnce([]() {
    auto const& current = internal::CurrentOptions();
    EXPECT_TRUE(current.has<EndpointOption>());
    EXPECT_TRUE(current.has<GrpcTracingOptionsOption>());
    EXPECT_TRUE(current.has<UserAgentProductsOption>());
    EXPECT_EQ(current.get<EndpointOption>(), "test-endpoint");
    EXPECT_EQ(current.get<GrpcTracingOptionsOption>()
                  .truncate_string_field_longer_than(),
              64);
    EXPECT_THAT(current.get<UserAgentProductsOption>(),
                Contains("test-only/1.0"));

    auto stream = absl::make_unique<MockAsyncStreamingReadWriteRpc>();
    EXPECT_CALL(*stream, Start).WillOnce([] {
      return make_ready_future(true);
    });
    EXPECT_CALL(*stream, Write)
        .WillOnce([](Request const& r, grpc::WriteOptions) {
          EXPECT_EQ(r.stream(), "test-only-request-stream");
          return make_ready_future(true);
        });
    EXPECT_CALL(*stream, Read)
        .WillOnce([] {
          Response response;
          response.set_response("test-only-response");
          return make_ready_future(absl::make_optional(response));
        })
        .WillOnce([] { return make_ready_future(absl::optional<Response>()); });
    EXPECT_CALL(*stream, Finish).WillOnce([] {
      return make_ready_future(Status(StatusCode::kUnavailable, "try-again"));
    });
    return stream;
  });
  GoldenKitchenSinkClient client(
      std::move(mock), Options{}
                           .set<EndpointOption>("test-endpoint")
                           .set<UserAgentProductsOption>({"override-me-too"}));
  auto stream = client.AsyncStreamingReadWrite(
      Options{}.set<UserAgentProductsOption>({"test-only/1.0"}));
  ASSERT_TRUE(stream->Start().get());
  Request request;
  request.set_stream("test-only-request-stream");
  ASSERT_TRUE(stream->Write(request, grpc::WriteOptions()).get());
  auto read = stream->Read().get();
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read->response(), "test-only-response");
  read = stream->Read().get();
  EXPECT_FALSE(read.has_value());
  EXPECT_THAT(stream->Finish().get(),
              StatusIs(StatusCode::kUnavailable, "try-again"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
