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

#include "google/cloud/internal/rest_parse_json_error.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Pair;

TEST(RestParseErrorInfoTest, Success) {
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

  auto constexpr kMessage = "API key not valid. Please pass a valid API key.";
  auto const error_info = ErrorInfo{
      "API_KEY_INVALID",
      "googleapis.com",
      {{"service", "translate.googleapis.com"}, {"http_status_code", "400"}}};
  EXPECT_THAT(ParseJsonError(400, kJsonPayload), Pair(kMessage, error_info));
}

TEST(RestParseErrorInfoTest, InvalidJson) {
  // Some valid json, but not what we're looking for.
  auto constexpr kJsonPayload = R"({"code":123, "message":"some message" })";
  EXPECT_THAT(ParseJsonError(400, kJsonPayload),
              Pair(kJsonPayload, ErrorInfo{}));
}

TEST(RestParseErrorInfoTest, InvalidOnlyString) {
  // Some valid json, but not what we're looking for.
  auto constexpr kJsonPayload = R"("uh-oh some error here")";
  EXPECT_THAT(ParseJsonError(400, kJsonPayload),
              Pair(kJsonPayload, ErrorInfo{}));
}

TEST(RestParseErrorInfoTest, InvalidUnexpectedFormat) {
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
    EXPECT_THAT(ParseJsonError(400, payload), Pair(payload, ErrorInfo{}));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
