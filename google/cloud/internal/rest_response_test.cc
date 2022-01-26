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

#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Return;

class MapHttpCodeToStatusTest
    : public ::testing::TestWithParam<std::pair<HttpStatusCode, StatusCode>> {};

TEST_P(MapHttpCodeToStatusTest, CorrectMapping) {
  EXPECT_THAT(GetParam().second, Eq(AsStatus(GetParam().first, "").code()));
}

INSTANTIATE_TEST_SUITE_P(
    MappingPairs, MapHttpCodeToStatusTest,
    testing::Values(
        std::make_pair(HttpStatusCode::kContinue, StatusCode::kOk),
        std::make_pair(static_cast<HttpStatusCode>(102), StatusCode::kOk),
        std::make_pair(HttpStatusCode::kOk, StatusCode::kOk),
        std::make_pair(HttpStatusCode::kCreated, StatusCode::kOk),
        std::make_pair(static_cast<HttpStatusCode>(202), StatusCode::kOk),
        std::make_pair(static_cast<HttpStatusCode>(303), StatusCode::kUnknown),
        std::make_pair(HttpStatusCode::kNotModified,
                       StatusCode::kFailedPrecondition),
        std::make_pair(HttpStatusCode::kResumeIncomplete,
                       StatusCode::kFailedPrecondition),
        std::make_pair(HttpStatusCode::kBadRequest,
                       StatusCode::kInvalidArgument),
        std::make_pair(HttpStatusCode::kUnauthorized,
                       StatusCode::kUnauthenticated),
        std::make_pair(HttpStatusCode::kForbidden,
                       StatusCode::kPermissionDenied),
        std::make_pair(HttpStatusCode::kNotFound, StatusCode::kNotFound),
        std::make_pair(HttpStatusCode::kMethodNotAllowed,
                       StatusCode::kPermissionDenied),
        std::make_pair(static_cast<HttpStatusCode>(406),
                       StatusCode::kInvalidArgument),
        std::make_pair(HttpStatusCode::kRequestTimeout,
                       StatusCode::kUnavailable),
        std::make_pair(HttpStatusCode::kConflict, StatusCode::kAborted),
        std::make_pair(HttpStatusCode::kGone, StatusCode::kNotFound),
        std::make_pair(HttpStatusCode::kLengthRequired,
                       StatusCode::kInvalidArgument),
        std::make_pair(HttpStatusCode::kPreconditionFailed,
                       StatusCode::kFailedPrecondition),
        std::make_pair(HttpStatusCode::kPayloadTooLarge,
                       StatusCode::kOutOfRange),
        std::make_pair(HttpStatusCode::kRequestRangeNotSatisfiable,
                       StatusCode::kOutOfRange),
        std::make_pair(HttpStatusCode::kTooManyRequests,
                       StatusCode::kUnavailable),
        std::make_pair(HttpStatusCode::kInternalServerError,
                       StatusCode::kUnavailable),
        std::make_pair(HttpStatusCode::kBadGateway, StatusCode::kUnavailable),
        std::make_pair(HttpStatusCode::kServiceUnavailable,
                       StatusCode::kUnavailable),
        std::make_pair(static_cast<HttpStatusCode>(504), StatusCode::kInternal),
        std::make_pair(static_cast<HttpStatusCode>(601), StatusCode::kUnknown)),
    [](testing::TestParamInfo<MapHttpCodeToStatusTest::ParamType> const& info) {
      return std::to_string(std::get<0>(info.param));
    });

TEST(AsStatus, RestResponseIsOk) {
  auto response = absl::make_unique<testing_util::MockRestResponse>();
  EXPECT_CALL(*response, StatusCode).WillOnce(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .WillOnce([]() -> std::unique_ptr<HttpPayload> {
        auto payload = absl::make_unique<testing_util::MockHttpPayload>();
        EXPECT_CALL(*payload, Read).WillOnce([](absl::Span<char>) {
          return 0;
        });
        return payload;
      });

  auto status = AsStatus(std::move(*response));
  EXPECT_THAT(status, IsOk());
}

TEST(AsStatus, RestResponseIsNotOkNoPayload) {
  auto response = absl::make_unique<testing_util::MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillOnce(Return(HttpStatusCode::kNotFound));

  EXPECT_CALL(std::move(*response), ExtractPayload)
      .WillOnce([]() -> std::unique_ptr<HttpPayload> {
        auto payload = absl::make_unique<testing_util::MockHttpPayload>();
        EXPECT_CALL(*payload, Read).WillOnce([](absl::Span<char>) {
          return 0;
        });
        return payload;
      });

  auto status = AsStatus(std::move(*response));
  EXPECT_THAT(status, StatusIs(StatusCode::kNotFound));
  EXPECT_THAT(status.message(), Eq("Received HTTP status code: 404"));
  EXPECT_TRUE(status.error_info().reason().empty());
}

TEST(AsStatus, RestResponseIsNotOkPayload) {
  std::string error = R"""(
  {
    "error": {
      "code": 404,
      "message": "API key not valid. Please pass a valid API key.",
      "status": "INVALID_ARGUMENT",
      "details": [
        {
          "@type": "type.googleapis.com/google.rpc.ErrorInfo",
          "reason": "API_KEY_INVALID",
          "domain": "googleapis.com",
          "metadata": {
            "service": "translate.googleapis.com"
          }
        }
      ]
    }
  })""";

  auto response = absl::make_unique<testing_util::MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillOnce(Return(HttpStatusCode::kNotFound));

  EXPECT_CALL(std::move(*response), ExtractPayload)
      .WillOnce([&]() -> std::unique_ptr<HttpPayload> {
        auto payload = absl::make_unique<testing_util::MockHttpPayload>();
        EXPECT_CALL(*payload, Read)
            .WillOnce([&](absl::Span<char> buffer) {
              std::copy(error.begin(), error.end(), buffer.begin());
              return error.size();
            })
            .WillOnce([](absl::Span<char>) { return 0; });
        return payload;
      });

  auto status = AsStatus(std::move(*response));
  EXPECT_THAT(status, StatusIs(StatusCode::kNotFound));
  EXPECT_THAT(status.message(),
              Eq("API key not valid. Please pass a valid API key."));
  EXPECT_THAT(status.error_info().reason(), Eq("API_KEY_INVALID"));
  EXPECT_THAT(status.error_info().domain(), Eq("googleapis.com"));
  EXPECT_THAT(status.error_info().metadata(),
              Contains(std::make_pair("service", "translate.googleapis.com")));
};

TEST(AsStatus, HttpStatusCodeIsOk) {
  auto status = AsStatus(HttpStatusCode::kOk, "");
  EXPECT_THAT(status, IsOk());
}

TEST(AsStatus, HttpStatusCodeIsNotOkNoPayload) {
  auto status = AsStatus(HttpStatusCode::kForbidden, "");
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_THAT(status.message(), Eq("Received HTTP status code: 403"));
}

TEST(AsStatus, HttpStatusCodeValidJsonButNotErrorCode) {
  auto status =
      AsStatus(HttpStatusCode::kForbidden, R"""("valid-but-not-error-code")""");
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_THAT(status.message(), Eq(R"""("valid-but-not-error-code")"""));
}

TEST(AsStatus, HttpStatusCodeNotFoundPayload) {
  std::string error = R"""(
  {
    "error": {
      "code": 404,
      "message": "API key not valid. Please pass a valid API key.",
      "status": "INVALID_ARGUMENT",
      "details": [
        {
          "@type": "type.googleapis.com/google.rpc.ErrorInfo",
          "reason": "API_KEY_INVALID",
          "domain": "googleapis.com",
          "metadata": {
            "service": "translate.googleapis.com"
          }
        }
      ]
    }
  })""";

  auto status = AsStatus(HttpStatusCode::kNotFound, error);
  EXPECT_THAT(status, StatusIs(StatusCode::kNotFound));
  EXPECT_THAT(status.message(),
              Eq("API key not valid. Please pass a valid API key."));
  EXPECT_THAT(status.error_info().reason(), Eq("API_KEY_INVALID"));
  EXPECT_THAT(status.error_info().domain(), Eq("googleapis.com"));
  EXPECT_THAT(status.error_info().metadata(),
              Contains(std::make_pair("service", "translate.googleapis.com")));
  EXPECT_THAT(status.error_info().metadata(),
              Contains(std::make_pair("http_status_code", "404")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
