// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
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
}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
