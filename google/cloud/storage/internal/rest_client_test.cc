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

#include "google/cloud/storage/internal/rest_client.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::Eq;

TEST(RestClientTest, ResolveStorageAuthorityProdEndpoint) {
  auto options =
      Options{}.set<RestEndpointOption>("https://storage.googleapis.com");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("storage.googleapis.com"));
}

TEST(RestClientTest, ResolveStorageAuthorityEapEndpoint) {
  auto options =
      Options{}.set<RestEndpointOption>("https://eap.googleapis.com");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("storage.googleapis.com"));
}

TEST(RestClientTest, ResolveStorageAuthorityNonGoogleEndpoint) {
  auto options = Options{}.set<RestEndpointOption>("https://localhost");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_FALSE(result_options.has<AuthorityOption>());
}

TEST(RestClientTest, ResolveStorageAuthorityOptionSpecified) {
  auto options = Options{}
                     .set<RestEndpointOption>("https://storage.googleapis.com")
                     .set<AuthorityOption>("auth_option_set");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(), Eq("auth_option_set"));
}

TEST(RestClientTest, ResolveIamAuthorityProdEndpoint) {
  auto options =
      Options{}.set<IamEndpointOption>("https://iamcredentials.googleapis.com");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("iamcredentials.googleapis.com"));
}

TEST(RestClientTest, ResolveIamAuthorityEapEndpoint) {
  auto options = Options{}.set<IamEndpointOption>("https://eap.googleapis.com");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("iamcredentials.googleapis.com"));
}

TEST(RestClientTest, ResolveIamAuthorityNonGoogleEndpoint) {
  auto options = Options{}.set<IamEndpointOption>("https://localhost");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_FALSE(result_options.has<AuthorityOption>());
}

TEST(RestClientTest, ResolveIamAuthorityOptionSpecified) {
  auto options =
      Options{}
          .set<IamEndpointOption>("https://iamcredentials.googleapis.com")
          .set<AuthorityOption>("auth_option_set");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(), Eq("auth_option_set"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
