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
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include <gmock/gmock.h>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockClient;
using ::google::cloud::testing_util::MockRestClient;
using ::testing::Eq;
using ::testing::MockFunction;

struct EndpointAuthorityTestValue {
  std::string storage_endpoint;
  std::string iam_endpoint;
  absl::optional<std::string> expected_storage_authority;
  absl::optional<std::string> expected_iam_authority;
  absl::optional<std::string> authority_option;
};

class CreateRestClientTest
    : public ::testing::TestWithParam<EndpointAuthorityTestValue> {};

TEST_P(CreateRestClientTest, CorrectClientParameters) {
  EndpointAuthorityTestValue test_param = GetParam();
  auto options = Options{}
                     .set<RestEndpointOption>(test_param.storage_endpoint)
                     .set<IamEndpointOption>(test_param.iam_endpoint);
  if (test_param.authority_option.has_value()) {
    options.set<AuthorityOption>(*test_param.authority_option);
  }

  MockFunction<std::unique_ptr<google::cloud::rest_internal::RestClient>(
      std::string, Options)>
      mock_rest_factory_fn;

  EXPECT_CALL(mock_rest_factory_fn, Call)
      .WillOnce([&](std::string const& endpoint, Options const& options) {
        EXPECT_THAT(endpoint, Eq(test_param.storage_endpoint));
        if (test_param.expected_storage_authority.has_value()) {
          EXPECT_THAT(options.get<AuthorityOption>(),
                      Eq(test_param.expected_storage_authority));
        } else {
          EXPECT_FALSE(options.has<AuthorityOption>());
        }
        return absl::make_unique<MockRestClient>();
      })
      .WillOnce([&](std::string const& endpoint, Options const& options) {
        EXPECT_THAT(endpoint, Eq(test_param.iam_endpoint));
        if (test_param.expected_iam_authority.has_value()) {
          EXPECT_THAT(options.get<AuthorityOption>(),
                      Eq(test_param.expected_iam_authority));
        } else {
          EXPECT_FALSE(options.has<AuthorityOption>());
        }
        return absl::make_unique<MockRestClient>();
      });

  MockFunction<std::shared_ptr<RawClient>(Options)> mock_curl_factory_fn;

  EXPECT_CALL(mock_curl_factory_fn, Call).WillOnce([&](Options const& options) {
    if (test_param.authority_option.has_value()) {
      EXPECT_THAT(options.get<AuthorityOption>(),
                  Eq(*test_param.authority_option));
    } else {
      EXPECT_FALSE(options.has<AuthorityOption>());
    }
    return std::make_shared<MockClient>();
  });

  auto client = RestClient::Create(std::move(options),
                                   mock_rest_factory_fn.AsStdFunction(),
                                   mock_curl_factory_fn.AsStdFunction());
}

INSTANTIATE_TEST_SUITE_P(
    CreateWithOptions, CreateRestClientTest,
    ::testing::Values(
        EndpointAuthorityTestValue{
            "https://storage.googleapis.com",
            "https://iamcredentials.googleapis.com", "storage.googleapis.com",
            "iamcredentials.googleapis.com", absl::nullopt},
        EndpointAuthorityTestValue{
            "https://eap.googleapis.com",
            "https://iamcredentials.googleapis.com", "storage.googleapis.com",
            "iamcredentials.googleapis.com", absl::nullopt},
        EndpointAuthorityTestValue{
            "https://storage.googleapis.com", "https://private.googleapis.com",
            "storage.googleapis.com", "iamcredentials.googleapis.com",
            absl::nullopt},
        EndpointAuthorityTestValue{
            "https://eap.googleapis.com", "https://private.googleapis.com",
            "storage.googleapis.com", "iamcredentials.googleapis.com",
            absl::nullopt},
        EndpointAuthorityTestValue{"https://storage.googleapis.com",
                                   "https://iamcredentials.googleapis.com",
                                   "authority_option", "authority_option",
                                   "authority_option"},
        EndpointAuthorityTestValue{
            "https://eap.googleapis.com", "https://private.googleapis.com",
            "authority_option", "authority_option", "authority_option"},
        EndpointAuthorityTestValue{"localhost", "::1", absl::nullopt,
                                   absl::nullopt, absl::nullopt},
        EndpointAuthorityTestValue{"localhost", "::1", "authority_option",
                                   "authority_option", "authority_option"}));

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
