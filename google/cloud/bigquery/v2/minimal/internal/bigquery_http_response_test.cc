// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/internal/bigquery_http_response.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(BigQueryHttpResponseTest, Success) {
  std::string job_response_payload = "My code my code my kingdom for code!";

  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(*mock_response, Headers)
      .WillRepeatedly(Return(std::multimap<std::string, std::string>()));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillRepeatedly(
          Return(ByMove(MakeMockHttpPayloadSuccess(job_response_payload))));

  auto http_response =
      BigQueryHttpResponse::BuildFromRestResponse(std::move(mock_response));
  ASSERT_STATUS_OK(http_response);
  EXPECT_TRUE(http_response->http_headers.empty());
  EXPECT_THAT(http_response->payload, Eq(job_response_payload));
}

TEST(BigQueryHttpResponseTest, HttpError) {
  auto mock_payload = std::make_unique<MockHttpPayload>();
  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));
  auto http_response =
      BigQueryHttpResponse::BuildFromRestResponse(std::move(mock_response));
  EXPECT_THAT(http_response, StatusIs(StatusCode::kInvalidArgument,
                                      HasSubstr("Received HTTP status code")));
}

TEST(BigQueryHttpResponseTest, PayloadError) {
  auto mock_payload = std::make_unique<MockHttpPayload>();
  EXPECT_CALL(*mock_payload, Read).WillRepeatedly([](absl::Span<char> const&) {
    return internal::AbortedError("invalid payload", GCP_ERROR_INFO());
  });

  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(std::move(mock_payload)));
  auto http_response =
      BigQueryHttpResponse::BuildFromRestResponse(std::move(mock_response));
  EXPECT_THAT(http_response,
              StatusIs(StatusCode::kAborted, HasSubstr("invalid payload")));
}

TEST(BigQueryHttpResponseTest, NullPtr) {
  auto http_response = BigQueryHttpResponse::BuildFromRestResponse(nullptr);
  EXPECT_FALSE(http_response.ok());
  EXPECT_THAT(http_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("RestResponse argument passed in is null")));
}

TEST(BigQueryHttpResponseTest, DebugString) {
  std::string payload = "some-payload";
  BigQueryHttpResponse response;
  response.http_status_code = HttpStatusCode::kOk;
  response.http_headers.insert({{"header1", "value1"}});
  response.payload = payload;

  EXPECT_EQ(response.DebugString("BigQueryHttpResponse", TracingOptions{}),
            R"(BigQueryHttpResponse {)"
            R"( status_code: 200)"
            R"( http_headers {)"
            R"( key: "header1")"
            R"( value: "value1")"
            R"( })"
            R"( payload: REDACTED)"
            R"( })");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
