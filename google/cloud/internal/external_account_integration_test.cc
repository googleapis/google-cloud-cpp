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

#include "google/cloud/internal/external_account_token_source_url.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;

TEST(ExternalAccountIntegrationTest, UrlSourced) {
  auto filename = GetEnv("GOOGLE_CLOUD_CPP_EXTERNAL_ACCOUNT_FILE");
  if (!filename.has_value()) GTEST_SKIP();
  auto is = std::ifstream(*filename);
  auto contents = std::string{std::istreambuf_iterator<char>{is.rdbuf()}, {}};
  ASSERT_FALSE(is.bad());
  ASSERT_FALSE(is.fail());
  // TODO(#5915) - use higher-level abstractions once available
  auto json = nlohmann::json::parse(contents, nullptr, false);
  ASSERT_TRUE(json.is_object()) << "json=" << json.dump();
  ASSERT_TRUE(json.contains("credential_source")) << "json=" << json.dump();
  auto credential_source = json["credential_source"];
  ASSERT_TRUE(credential_source.is_object()) << "json=" << json.dump();
  auto make_client = []() {
    return rest_internal::MakeDefaultRestClient("", Options{});
  };
  auto source = MakeExternalAccountTokenSourceUrl(
      credential_source, make_client,
      internal::ErrorContext(
          {{"GOOGLE_CLOUD_CPP_EXTERNAL_ACCOUNT_FILE", *filename},
           {"program", "test"}}));
  ASSERT_STATUS_OK(source);
  auto subject_token = (*source)(Options{});
  ASSERT_STATUS_OK(subject_token);
  std::cout << "subject_token=" << subject_token->token.substr(0, 32)
            << "...<truncated>...\n";
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
