// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/minimal_iam_credentials_rest.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;

TEST(MinimalIamCredentialsRestIntegrationTest, GetAccessToken) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto iam_service_account = google::cloud::internal::GetEnv(
                                 "GOOGLE_CLOUD_CPP_IAM_TEST_SERVICE_ACCOUNT")
                                 .value_or("");
  ASSERT_FALSE(iam_service_account.empty());

  auto credentials = oauth2::GoogleDefaultCredentials();
  ASSERT_THAT(credentials, IsOk());

  auto request = GenerateAccessTokenRequest{
      /*.service_account=*/iam_service_account,
      /*.lifetime=*/std::chrono::minutes(15),
      /*.scopes=*/{"https://www.googleapis.com/auth/devstorage.full_control"},
      /*.delegates=*/{},
  };

  auto stub = MakeMinimalIamCredentialsRestStub(
      *std::move(credentials), Options{}.set<TracingComponentsOption>({"rpc"}));

  // The stub does not implement a retry loop, to avoid flaky tests, we run one
  // manually.
  auto backoff = std::chrono::seconds(2);
  for (int i = 0; i != 5; ++i) {
    auto token = stub->GenerateAccessToken(request);
    if (token) {
      SUCCEED();
      return;
    }
    std::this_thread::sleep_for(backoff);
    backoff *= 2;
  }
  FAIL();
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
