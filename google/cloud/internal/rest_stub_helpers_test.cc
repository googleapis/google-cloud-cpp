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
#include <google/longrunning/operations.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
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

using Request = google::protobuf::Duration;
using Response = google::protobuf::Timestamp;

auto constexpr kMalformedJsonResponsePayload = R"(
  {
    "seconds":123,
    "nanos":"not-a-number"
  }
)";

TEST(RestStubHelpers, RestResponseToProtoErrorInfo) {
  auto mock_200_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(
        std::string(kMalformedJsonResponsePayload));
  });

  Response response;
  auto status = RestResponseToProto(response, std::move(*mock_200_response));
  EXPECT_THAT(status, Not(IsOk()));
  EXPECT_THAT(status.error_info().reason(),
              Eq("Failure creating proto Message from Json"));
  EXPECT_THAT(status.error_info().metadata(),
              Contains(Pair("message_type", "google.protobuf.Timestamp")));
  EXPECT_THAT(status.error_info().metadata(),
              Contains(Pair("json_string", kMalformedJsonResponsePayload)));
}

auto constexpr kJsonResponsePayloadWithUnknownField = R"(
  {
    "seconds":123,
    "nanos":456,
    "my_unknown_field":"unknown"
  }
)";

TEST(RestStubHelpers, RestResponseToProtoContainsUnknownField) {
  auto mock_200_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(
        std::string(kJsonResponsePayloadWithUnknownField));
  });

  Response response;
  auto status = RestResponseToProto(response, std::move(*mock_200_response));
  ASSERT_THAT(status, IsOk());
  EXPECT_THAT(response.seconds(), Eq(123));
  EXPECT_THAT(response.nanos(), Eq(456));
}

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

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Delete)
      .WillOnce([&](RestContext&, RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      })
      .WillOnce([&](RestContext&, RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext&, RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return std::unique_ptr<rest_internal::RestResponse>(mock_403_response);
      });

  RestContext context;
  Request request;
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

auto constexpr kJsonResponsePayload = R"(
  {
    "seconds":123,
    "nanos":456
  }
)";

TEST(RestStubHelpers, DeleteWithNonEmptyResponse) {
  std::string json_response(kJsonResponsePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Delete)
      .WillOnce([&](RestContext&, RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext&, RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/delete/"));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  Request request;
  auto result = Delete<Response>(*mock_client, context, request, "/v1/delete/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Delete<Response>(*mock_client, context, request, "/v1/delete/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->seconds(), Eq(123));
  EXPECT_THAT(result->nanos(), Eq(456));
}

TEST(RestStubHelpers, Get) {
  std::string json_response(kJsonResponsePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::protobuf::Timestamp proto_request;
  proto_request.set_seconds(123);

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Get)
      .WillOnce([&](RestContext&, RestRequest const& request) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("seconds"),
                    Contains(std::to_string(proto_request.seconds())));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext&, RestRequest const& request)
                    -> google::cloud::StatusOr<
                        std::unique_ptr<rest_internal::RestResponse>> {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("seconds"),
                    Contains(std::to_string(proto_request.seconds())));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  std::vector<std::pair<std::string, std::string>> params = {
      {"seconds", std::to_string(proto_request.seconds())}};
  auto result =
      Get<Response>(*mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Get<Response>(*mock_client, context, proto_request, "/v1/", params);
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->seconds(), Eq(123));
  EXPECT_THAT(result->nanos(), Eq(456));
}

auto constexpr kJsonUpdatePayload =
    R"({"response_type":"response_value","metadata_type":"metadata_value"})";

auto constexpr kJsonUpdateResponse = R"(
  {
    "seconds":123,
    "nanos":456
  }
)";

TEST(RestStubHelpers, ProtoRequestToJsonPayloadSuccess) {
  std::string json_payload;
  google::longrunning::OperationInfo proto_request;
  proto_request.set_response_type("response_value");
  proto_request.set_metadata_type("metadata_value");

  auto status = ProtoRequestToJsonPayload(proto_request, json_payload);
  ASSERT_THAT(status, IsOk());
  EXPECT_THAT(json_payload, Eq(kJsonUpdatePayload));
}

TEST(RestStubHelpers, Patch) {
  std::string json_response(kJsonUpdateResponse);
  std::string json_request(kJsonUpdatePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::longrunning::OperationInfo proto_request;
  proto_request.set_response_type("response_value");
  proto_request.set_metadata_type("metadata_value");

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Patch)
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext& context, RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        EXPECT_THAT(context.GetHeader("custom"), Contains("header"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  auto result = Patch<Response>(*mock_client, context, proto_request, "/v1/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  context.AddHeader("custom", "header");
  result = Patch<Response>(*mock_client, context, proto_request, "/v1/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->seconds(), Eq(123));
  EXPECT_THAT(result->nanos(), Eq(456));
}

TEST(RestStubHelpers, PostWithNonEmptyResponse) {
  std::string json_response(kJsonUpdateResponse);
  std::string json_request(kJsonUpdatePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::longrunning::OperationInfo proto_request;
  proto_request.set_response_type("response_value");
  proto_request.set_metadata_type("metadata_value");

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client,
              Post(_, _, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("response_type"),
                    Contains(proto_request.response_type()));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload)
                    -> google::cloud::StatusOr<
                        std::unique_ptr<rest_internal::RestResponse>> {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("response_type"),
                    Contains(proto_request.response_type()));
        EXPECT_THAT(request.GetQueryParameter("foo"), Contains("bar"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  std::vector<std::pair<std::string, std::string>> params = {
      {"response_type", proto_request.response_type()}, {"foo", "bar"}};
  auto result =
      Post<Response>(*mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Post<Response>(*mock_client, context, proto_request, "/v1/", params);
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->seconds(), Eq(123));
  EXPECT_THAT(result->nanos(), Eq(456));
}

TEST(RestStubHelpers, PostWithEmptyResponse) {
  std::string json_request(kJsonUpdatePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(std::string{});
  });

  google::longrunning::OperationInfo proto_request;
  proto_request.set_response_type("response_value");
  proto_request.set_metadata_type("metadata_value");

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client,
              Post(_, _, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("response_type"),
                    Contains(proto_request.response_type()));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetQueryParameter("response_type"),
                    Contains(proto_request.response_type()));
        EXPECT_THAT(request.GetQueryParameter("foo"), Contains("bar"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  std::vector<std::pair<std::string, std::string>> params = {
      {"response_type", proto_request.response_type()}, {"foo", "bar"}};
  auto result = Post(*mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Post(*mock_client, context, proto_request, "/v1/", params);
  EXPECT_THAT(result, IsOk());
}

TEST(RestStubHelpers, Put) {
  std::string json_response(kJsonUpdateResponse);
  std::string json_request(kJsonUpdatePayload);
  auto* mock_200_response = new MockRestResponse();
  EXPECT_CALL(*mock_200_response, StatusCode()).WillOnce([]() {
    return HttpStatusCode::kOk;
  });
  EXPECT_CALL(std::move(*mock_200_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(json_response);
  });

  google::longrunning::OperationInfo proto_request;
  proto_request.set_response_type("response_value");
  proto_request.set_metadata_type("metadata_value");

  auto mock_client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_client, Put)
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const&) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        return Status(StatusCode::kInternal, "Internal Error");
      })
      .WillOnce([&](RestContext&, RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(), Eq("/v1/"));
        EXPECT_THAT(request.GetHeader("content-type"),
                    Contains("application/json"));
        std::string payload_str(payload[0].begin(), payload[0].end());
        EXPECT_THAT(payload_str, Eq(json_request));
        return std::unique_ptr<rest_internal::RestResponse>(mock_200_response);
      });

  RestContext context;
  auto result = Put<Response>(*mock_client, context, proto_request, "/v1/");
  EXPECT_THAT(result, StatusIs(StatusCode::kInternal));

  result = Put<Response>(*mock_client, context, proto_request, "/v1/");
  ASSERT_THAT(result, IsOk());
  EXPECT_THAT(result->seconds(), Eq(123));
  EXPECT_THAT(result->nanos(), Eq(456));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
