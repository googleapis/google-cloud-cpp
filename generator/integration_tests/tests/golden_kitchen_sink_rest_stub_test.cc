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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_rest_stub.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpPayload;
using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::testing::_;
using ::testing::A;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;

auto constexpr kServiceUnavailable = "503 Service Unavailable";

class GoldenKitchenSinkRestStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rest_client_ = absl::make_unique<MockRestClient>();
    service_unavailable_ = kServiceUnavailable;
  }

  static std::unique_ptr<MockRestResponse> CreateMockRestResponse(
      std::string const& json_response,
      HttpStatusCode http_status_code = HttpStatusCode::kOk) {
    auto mock_response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*mock_response, StatusCode()).WillOnce([=] {
      return http_status_code;
    });
    EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
      auto mock_payload = absl::make_unique<MockHttpPayload>();
      EXPECT_CALL(*mock_payload, Read)
          .WillOnce([&](absl::Span<char> buffer) {
            std::copy(json_response.begin(), json_response.end(),
                      buffer.begin());
            return json_response.size();
          })
          .WillOnce([&](absl::Span<char>) { return 0; });
      return std::unique_ptr<HttpPayload>(std::move(mock_payload));
    });
    return mock_response;
  }

  std::unique_ptr<MockRestClient> mock_rest_client_;
  std::string service_unavailable_;
};

// This first test has a lot of overlap with the unit tests in
// rest_stub_helpers_test.cc just to make sure code generation works on both
// success and failure paths. Subsequent tests only check what the stub code
// affects and do not duplicate testing whether the HTTP helper methods work as
// they are tested elsewhere.
TEST_F(GoldenKitchenSinkRestStubTest, GenerateAccessToken) {
  auto constexpr kJsonRequestPayload =
      R"({"name":"projects/my_project/serviceAccounts/my_sa","scope":["scope1","scope2"]})";
  auto constexpr kJsonResponsePayload = R"({"access_token":"my_token"})";
  std::string json_request(kJsonRequestPayload);
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;

  auto mock_503_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_503_response, StatusCode()).WillRepeatedly([]() {
    return HttpStatusCode::kServiceUnavailable;
  });
  EXPECT_CALL(std::move(*mock_503_response), ExtractPayload).WillOnce([&] {
    auto mock_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(service_unavailable_.begin(), service_unavailable_.end(),
                    buffer.begin());
          return service_unavailable_.size();
        })
        .WillOnce([&](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_payload));
  });

  google::test::admin::database::v1::GenerateAccessTokenRequest proto_request;
  proto_request.set_name("projects/my_project/serviceAccounts/my_sa");
  *proto_request.add_scope() = "scope1";
  *proto_request.add_scope() = "scope2";

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce(
          [&](RestRequest const&, std::vector<absl::Span<char const>> const&) {
            return std::unique_ptr<rest_internal::RestResponse>(
                mock_503_response.release());
          })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/serviceAccounts/"
                       "my_sa:generateAccessToken"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto failure = stub.GenerateAccessToken(rest_context, proto_request);
  EXPECT_EQ(failure.status(),
            Status(StatusCode::kUnavailable, kServiceUnavailable));
  auto success = stub.GenerateAccessToken(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->access_token(), Eq("my_token"));
}

TEST_F(GoldenKitchenSinkRestStubTest, GenerateIdToken) {
  auto constexpr kJsonResponsePayload = R"({"token":"my_token"})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::GenerateIdTokenRequest proto_request;

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/token:generate"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.GenerateIdToken(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->token(), Eq("my_token"));
}

TEST_F(GoldenKitchenSinkRestStubTest, WriteLogEntries) {
  auto constexpr kJsonResponsePayload = R"({})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::WriteLogEntriesRequest proto_request;

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v2/entries:write"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.WriteLogEntries(rest_context, proto_request);
  EXPECT_THAT(success, IsOk());
}

TEST_F(GoldenKitchenSinkRestStubTest, ListLogs) {
  auto constexpr kJsonResponsePayload =
      R"({"log_names":["foo","bar"],"next_page_token":"my_next_page_token"})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::ListLogsRequest proto_request;
  proto_request.set_parent("projects/my_project");
  proto_request.set_page_token("my_page_token");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v2/projects/my_project/logs"));
        EXPECT_THAT(request.GetQueryParameter("page_token"),
                    Contains("my_page_token"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.ListLogs(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->log_names(), ElementsAre("foo", "bar"));
  EXPECT_THAT(success->next_page_token(), Eq("my_next_page_token"));
}

TEST_F(GoldenKitchenSinkRestStubTest, ListServiceAccountKeys) {
  auto constexpr kJsonResponsePayload = R"({"keys":["foo","bar"]})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest
      proto_request;
  proto_request.set_name("projects/my_project/serviceAccounts/my_sa");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(),
                    Eq("/v1/projects/my_project/serviceAccounts/my_sa/keys"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.ListServiceAccountKeys(rest_context, proto_request);
  ASSERT_THAT(success, IsOk());
  EXPECT_THAT(success->keys(), ElementsAre("foo", "bar"));
}

TEST_F(GoldenKitchenSinkRestStubTest, DoNothing) {
  auto constexpr kJsonResponsePayload = R"({})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::protobuf::Empty proto_request;

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/doNothing"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.DoNothing(rest_context, proto_request);
  EXPECT_THAT(success, IsOk());
}

TEST_F(GoldenKitchenSinkRestStubTest, ExplicitRouting1) {
  auto constexpr kJsonResponsePayload = R"({})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::ExplicitRoutingRequest proto_request;
  proto_request.set_table_name("tables/my_table");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/tables/my_table:explicitRouting1"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.ExplicitRouting1(rest_context, proto_request);
  EXPECT_THAT(success, IsOk());
}

TEST_F(GoldenKitchenSinkRestStubTest, ExplicitRouting2) {
  auto constexpr kJsonResponsePayload = R"({})";
  std::string json_response(kJsonResponsePayload);
  RestContext rest_context;
  google::test::admin::database::v1::ExplicitRoutingRequest proto_request;
  proto_request.set_table_name("tables/my_table");

  auto mock_200_response = CreateMockRestResponse(json_response);
  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/tables/my_table:explicitRouting2"));
        return std::unique_ptr<rest_internal::RestResponse>(
            mock_200_response.release());
      });
  DefaultGoldenKitchenSinkRestStub stub(std::move(mock_rest_client_), {});
  auto success = stub.ExplicitRouting2(rest_context, proto_request);
  EXPECT_THAT(success, IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
