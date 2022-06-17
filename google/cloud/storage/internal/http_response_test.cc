// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

TEST(HttpResponseTest, OStream) {
  HttpResponse response{
      404, "some-payload", {{"header1", "value1"}, {"header2", "value2"}}};

  std::ostringstream os;
  os << response;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("404"));
  EXPECT_THAT(actual, HasSubstr("some-payload"));
  EXPECT_THAT(actual, HasSubstr("header1: value1"));
  EXPECT_THAT(actual, HasSubstr("header2: value2"));
}

TEST(HttpResponseTest, AsStatus) {
  EXPECT_THAT(AsStatus(HttpResponse{-42, "weird", {}}),
              StatusIs(StatusCode::kUnknown));
  EXPECT_THAT(AsStatus(HttpResponse{99, "still weird", {}}),
              StatusIs(StatusCode::kUnknown));
  EXPECT_STATUS_OK(AsStatus(HttpResponse{100, "Continue", {}}));
  EXPECT_STATUS_OK(AsStatus(HttpResponse{200, "success", {}}));
  EXPECT_STATUS_OK(AsStatus(HttpResponse{299, "success", {}}));
  EXPECT_THAT(AsStatus(HttpResponse{300, "libcurl should handle this", {}}),
              StatusIs(StatusCode::kUnknown));
  EXPECT_THAT(AsStatus(HttpResponse{304, "nothing changed", {}}),
              StatusIs(StatusCode::kFailedPrecondition));
  EXPECT_THAT(AsStatus(HttpResponse{308, "pending", {}}),
              StatusIs(StatusCode::kFailedPrecondition));
  EXPECT_THAT(AsStatus(HttpResponse{400, "invalid something", {}}),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(AsStatus(HttpResponse{401, "unauthenticated", {}}),
              StatusIs(StatusCode::kUnauthenticated));
  EXPECT_THAT(AsStatus(HttpResponse{403, "forbidden", {}}),
              StatusIs(StatusCode::kPermissionDenied));
  EXPECT_THAT(AsStatus(HttpResponse{404, "not found", {}}),
              StatusIs(StatusCode::kNotFound));
  EXPECT_THAT(AsStatus(HttpResponse{405, "method not allowed", {}}),
              StatusIs(StatusCode::kPermissionDenied));
  EXPECT_THAT(AsStatus(HttpResponse{408, "request timeout", {}}),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(AsStatus(HttpResponse{409, "conflict", {}}),
              StatusIs(StatusCode::kAborted));
  EXPECT_THAT(AsStatus(HttpResponse{410, "gone", {}}),
              StatusIs(StatusCode::kNotFound));
  EXPECT_THAT(AsStatus(HttpResponse{411, "length required", {}}),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(AsStatus(HttpResponse{412, "precondition failed", {}}),
              StatusIs(StatusCode::kFailedPrecondition));
  EXPECT_THAT(AsStatus(HttpResponse{413, "payload too large", {}}),
              StatusIs(StatusCode::kOutOfRange));
  EXPECT_THAT(AsStatus(HttpResponse{416, "request range", {}}),
              StatusIs(StatusCode::kOutOfRange));
  EXPECT_THAT(AsStatus(HttpResponse{429, "too many requests", {}}),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(AsStatus(HttpResponse{499, "some 4XX error", {}}),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(AsStatus(HttpResponse{500, "internal server error", {}}),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(AsStatus(HttpResponse{502, "bad gateway", {}}),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(AsStatus(HttpResponse{503, "service unavailable", {}}),
              StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(AsStatus(HttpResponse{599, "some 5XX error", {}}),
              StatusIs(StatusCode::kInternal));
  EXPECT_THAT(AsStatus(HttpResponse{600, "bad", {}}),
              StatusIs(StatusCode::kUnknown));
}

TEST(HttpResponseTest, ErrorInfo) {
  // This example payload comes from
  // https://cloud.google.com/apis/design/errors#http_mapping
  auto constexpr kJsonPayload = R"(
    {
      "error": {
        "code": 400,
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
    }
  )";

  ErrorInfo error_info{
      "API_KEY_INVALID",
      "googleapis.com",
      {{"service", "translate.googleapis.com"}, {"http_status_code", "400"}}};
  std::string message = "API key not valid. Please pass a valid API key.";
  Status expected{StatusCode::kInvalidArgument, message, error_info};
  EXPECT_EQ(AsStatus(HttpResponse{400, kJsonPayload, {}}), expected);
}

TEST(HttpResponseTest, ErrorInfoInvalidJson) {
  // Some valid json, but not what we're looking for.
  auto constexpr kJsonPayload = R"({"code":123, "message":"some message" })";
  Status expected{StatusCode::kInvalidArgument, kJsonPayload};
  EXPECT_EQ(AsStatus(HttpResponse{400, kJsonPayload, {}}), expected);
}

TEST(HttpResponseTest, ErrorInfoInvalidOnlyString) {
  // Some valid json, but not what we're looking for.
  auto constexpr kJsonPayload = R"("uh-oh some error here")";
  Status expected{StatusCode::kInvalidArgument, kJsonPayload};
  EXPECT_EQ(AsStatus(HttpResponse{400, kJsonPayload, {}}), expected);
}

TEST(HttpResponseTest, ErrorInfoInvalidUnexpectedFormat) {
  std::string cases[] = {
      R"js({"error": "invalid_grant", "error_description": "Invalid grant: account not found"})js",
      R"js({"error": ["invalid"], "error_description": "Invalid grant: account not found"})js",
      R"js({"error": {"missing-message": "msg"}})js",
      R"js({"error": {"message": "msg", "missing-details": {}}})js",
      R"js({"error": {"message": ["not string"], "details": {}}}})js",
      R"js({"error": {"message": "the error", "details": "not-an-array"}}})js",
      R"js({"error": {"message": "the error", "details": {"@type": "invalid-@type"}}}})js",
      R"js({"error": {"message": "the error", "details": ["not-an-object"]}}})js",
      R"js({"error": {"message": "the error", "details": [{"@type": "invalid-@type"}]}}})js",
      R"js(Service Unavailable)js",
      R"js("Service Unavailable")js",
  };
  for (auto const& payload : cases) {
    Status expected{StatusCode::kInvalidArgument, payload};
    EXPECT_EQ(AsStatus(HttpResponse{400, payload, {}}), expected);
  }
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
