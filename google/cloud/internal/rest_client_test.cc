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

#include "google/cloud/internal/rest_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;

TEST(HostHeader, OptionNotSet) {
  auto result = CurlRestClient::HostHeader({}, "endpoint.foo.com");
  EXPECT_TRUE(result.empty());
}

TEST(HostHeader, OptionSetNotGoogleapis) {
  auto result = CurlRestClient::HostHeader(
      Options{}.set<RestEndpointOption>("private.foo.com"), "endpoint.foo.com");
  EXPECT_TRUE(result.empty());
}

TEST(HostHeader, OptionSetGoogleapis) {
  auto result = CurlRestClient::HostHeader(
      Options{}.set<RestEndpointOption>("private.googleapis.com"),
      "endpoint.googleapis.com");
  EXPECT_THAT(result, Eq("Host: endpoint.googleapis.com"));
}

TEST(HostHeader, OptionSetGoogleapisMisformatted) {
  auto result = CurlRestClient::HostHeader(
      Options{}.set<RestEndpointOption>("private.googleapis.com"),
      "https://endpoint.googleapis.com");
  EXPECT_THAT(result, Eq("Host: endpoint.googleapis.com"));
}

TEST(HostHeader, OptionSetGoogleapisMisformattedAgain) {
  auto result = CurlRestClient::HostHeader(
      Options{}.set<RestEndpointOption>("private.googleapis.com"),
      "https://endpoint.googleapis.com/v1/");
  EXPECT_THAT(result, Eq("Host: endpoint.googleapis.com"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
