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

TEST(AsStatus, HttpStatusCodeIsOk) {
  auto status = AsStatus(HttpStatusCode::kOk, "");
  EXPECT_THAT(status, IsOk());
}

TEST(AsStatus, HttpStatusCodeIsNotOkNoPayload) {
  auto status = AsStatus(HttpStatusCode::kForbidden, "");
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_THAT(status.message(), Eq("Received HTTP status code: 403"));
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
