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

#include "google/cloud/internal/rest_stub_helpers.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/iam/admin/v1/iam.pb.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::A;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Pair;

auto constexpr kJsonErrorPayload = R"(
    {
      "error": {
        "code": 403,
        "message": "Permission foo denied on resource bar.",
        "status": "PERMISSION_DENIED",
        "details": [
          {
            "@type": "type.googleapis.com/google.rpc.ErrorInfo",
            "reason": "PERMISSION_DENIED",
            "domain": "googleapis.com",
            "metadata": {
              "service": "iam.googleapis.com"
            }
          }
        ]
      }
    }
)";

TEST(RestStubHelpers, DeleteWithEmptyResponse) {
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(std::string{});
  });

  std::string json_error(kJsonErrorPayload);
  auto* mock_403_response = new MockRestResponse();
  EXPECT_CALL(*mock_403_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kForbidden;
  });
  EXPECT_CALL(std::move(*mock_403_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_error);
  });

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Delete)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      })
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return std::unique_ptr<rest_internal::RestResponse>(mock_403_response);
      });

  RestContext context;
  google::iam::admin::v1::DeleteServiceAccountRequest request;
  auto result = Delete(*mock_client, context, request, "/v1/delete/");
  EXPECT_THAT(result, IsOk());

  result = Delete(*mock_client, context, request, "/v1/delete/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Delete(*mock_client, context, request, "/v1/delete/");
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_THAT(result.message(), Eq("Permission foo denied on resource bar."));
  EXPECT_THAT(result.error_info().domain(), Eq("googleapis.com"));
  EXPECT_THAT(result.error_info().reason(), Eq("PERMISSION_DENIED"));
  EXPECT_THAT(result.error_info().metadata(),
              Contains(Pair("service", "iam.googleapis.com")));
}

auto constexpr kJsonRolePayload = R"(
  {
    "name":"role_name",
    "title":"role_title",
    "description":"role_description"
  }
)";

TEST(RestStubHelpers, DeleteWithNonEmptyResponse) {
  std::string json_response(kJsonRolePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Delete)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  google::iam::admin::v1::DeleteRoleRequest request;
  auto result = Delete<google::iam::admin::v1::Role>(*mock_client, context,
                                                     request, "/v1/delete/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Delete<google::iam::admin::v1::Role>(*mock_client, context, request,
                                                "/v1/delete/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->name(), Eq("role_name"));
  EXPECT_THAT(result->title(), Eq("role_title"));
}

TEST(RestStubHelpers, Get) {
  std::string json_response(kJsonRolePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::iam::admin::v1::GetRoleRequest proto_request;
  proto_request.set_name("role_name");

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("name"),
                    Contains(proto_request.name()));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request)
                    -> google::cloud::StatusOr<
                        std::unique_ptr<rest_internal::RestResponse>> {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("name"),
                    Contains(proto_request.name()));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  std::vector<std::pair<std::string, std::string>> params = {
      {"name", proto_request.name()}};
  auto result = Get<google::iam::admin::v1::Role>(
      *mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Get<google::iam::admin::v1::Role>(*mock_client, context,
                                             proto_request, "/v1/", params);
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->name(), Eq("role_name"));
  EXPECT_THAT(result->title(), Eq("role_title"));
}

auto constexpr kJsonUpdateRolePayload =
    R"({"name":"projects/project_name/roles/role_name","role":{"name":"update_role_name","title":"update_role_title","description":"update_role_description"}})";

auto constexpr kJsonUpdateRoleResponse = R"(
  {
    "name":"update_role_name",
    "title":"update_role_title",
    "description":"update_role_description"
  }
)";

TEST(RestStubHelpers, ProtoRequestToJsonPayloadSuccess) {
  std::string json_payload;
  google::iam::admin::v1::UpdateRoleRequest proto_request;
  proto_request.set_name("projects/project_name/roles/role_name");
  google::iam::admin::v1::Role update_role;
  update_role.set_name("update_role_name");
  update_role.set_title("update_role_title");
  update_role.set_description("update_role_description");
  *proto_request.mutable_role() = update_role;

  auto status = ProtoRequestToJsonPayload(proto_request, json_payload);
  ASSERT_THAT(status, IsOk());
  EXPECT_THAT(json_payload, Eq(kJsonUpdateRolePayload));
}

TEST(RestStubHelpers, Patch) {
  std::string json_response(kJsonUpdateRoleResponse);
  std::string json_request(kJsonUpdateRolePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::iam::admin::v1::UpdateRoleRequest proto_request;
  proto_request.set_name("projects/project_name/roles/role_name");
  google::iam::admin::v1::Role update_role;
  update_role.set_name("update_role_name");
  update_role.set_title("update_role_title");
  update_role.set_description("update_role_description");
  *proto_request.mutable_role() = update_role;

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Patch)
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        EXPECT_THAT(request.GetHeader("custom"), Contains("header"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  auto result = Patch<google::iam::admin::v1::Role>(*mock_client, context,
                                                    proto_request, "/v1/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  context.AddHeader("custom", "header");
  result = Patch<google::iam::admin::v1::Role>(*mock_client, context,
                                               proto_request, "/v1/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->name(), Eq("update_role_name"));
  EXPECT_THAT(result->title(), Eq("update_role_title"));
}

TEST(RestStubHelpers, PostWithNonEmptyResponse) {
  std::string json_response(kJsonUpdateRoleResponse);
  std::string json_request(kJsonUpdateRolePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::iam::admin::v1::UpdateRoleRequest proto_request;
  proto_request.set_name("projects/project_name/roles/role_name");
  google::iam::admin::v1::Role update_role;
  update_role.set_name("update_role_name");
  update_role.set_title("update_role_title");
  update_role.set_description("update_role_description");
  *proto_request.mutable_role() = update_role;

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("name"),
                    Contains(proto_request.name()));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload)
                    -> google::cloud::StatusOr<
                        std::unique_ptr<rest_internal::RestResponse>> {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("name"),
                    Contains(proto_request.name()));
        EXPECT_THAT(request.GetQueryParameter("foo"), Contains("bar"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  std::vector<std::pair<std::string, std::string>> params = {
      {"name", proto_request.name()}, {"foo", "bar"}};
  auto result = Post<google::iam::admin::v1::Role>(
      *mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Post<google::iam::admin::v1::Role>(*mock_client, context,
                                              proto_request, "/v1/", params);
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->name(), Eq("update_role_name"));
  EXPECT_THAT(result->title(), Eq("update_role_title"));
}

TEST(RestStubHelpers, PostWithEmptyResponse) {
  std::string json_request(kJsonUpdateRolePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(std::string{});
  });

  google::iam::admin::v1::UpdateRoleRequest proto_request;
  proto_request.set_name("projects/project_name/roles/role_name");
  google::iam::admin::v1::Role update_role;
  update_role.set_name("update_role_name");
  update_role.set_title("update_role_title");
  update_role.set_description("update_role_description");
  *proto_request.mutable_role() = update_role;

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("name"),
                    Contains(proto_request.name()));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("name"),
                    Contains(proto_request.name()));
        EXPECT_THAT(request.GetQueryParameter("foo"), Contains("bar"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  std::vector<std::pair<std::string, std::string>> params = {
      {"name", proto_request.name()}, {"foo", "bar"}};
  auto result = Post(*mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Post(*mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, IsOk());
}

TEST(RestStubHelpers, Put) {
  std::string json_response(kJsonUpdateRoleResponse);
  std::string json_request(kJsonUpdateRolePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::iam::admin::v1::UpdateRoleRequest proto_request;
  proto_request.set_name("projects/project_name/roles/role_name");
  google::iam::admin::v1::Role update_role;
  update_role.set_name("update_role_name");
  update_role.set_title("update_role_title");
  update_role.set_description("update_role_description");
  *proto_request.mutable_role() = update_role;

  auto mock_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Put)
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  auto result = Put<google::iam::admin::v1::Role>(*mock_client, context,
                                                  proto_request, "/v1/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Put<google::iam::admin::v1::Role>(*mock_client, context,
                                             proto_request, "/v1/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->name(), Eq("update_role_name"));
  EXPECT_THAT(result->title(), Eq("update_role_title"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
