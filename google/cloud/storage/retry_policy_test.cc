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

#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/internal/http_response.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(RetryPolicyTest, PermanentFailure) {
  // https://cloud.google.com/storage/docs/json_api/v1/status-codes
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{401, "unauthorized", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{403, "forbidden", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{404, "not found", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{409, "conflict", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{410, "gone", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{411, "length required", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{412, "precondition failed", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{413, "payload too large", {}})));
  EXPECT_TRUE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{416, "request not satisfiable", {}})));
  EXPECT_FALSE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{429, "too many requests", {}})));
  EXPECT_FALSE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{500, "internal server error", {}})));
  EXPECT_FALSE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{502, "bad gateway", {}})));
  EXPECT_FALSE(StatusTraits::IsPermanentFailure(
      AsStatus(HttpResponse{503, "service unavailable", {}})));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
