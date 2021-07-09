// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/self_signing_service_account_credentials.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/testing/constants.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::StartsWith;

TEST(SelfSigningServiceAccountCredentials, CreateBearerToken) {
  auto const info = SelfSigningServiceAccountCredentialsInfo{
      /*.client_email*/ "--invalid--@developer.gserviceaccount.com",
      /*.private_key_id*/ "test-private-key-id",
      /*.private_key*/ testing::kWellFormatedKey,
      /*.audience*/ "https://storage.googleapis.com",
  };
  auto const now = std::chrono::system_clock::now();
  auto actual = CreateBearerToken(info, now);
  ASSERT_THAT(actual, IsOk());
  std::vector<std::string> components = absl::StrSplit(*actual, '.');
  std::vector<std::string> decoded(components.size());
  std::transform(components.begin(), components.end(), decoded.begin(),
                 [](std::string const& e) {
                   auto v = UrlsafeBase64Decode(e).value();
                   return std::string{v.begin(), v.end()};
                 });
  ASSERT_THAT(3, decoded.size());
  auto const header = nlohmann::json::parse(decoded[0], nullptr);
  ASSERT_FALSE(header.is_null()) << "header=" << decoded[0];
  auto const payload = nlohmann::json::parse(decoded[1], nullptr);
  ASSERT_FALSE(payload.is_null()) << "payload=" << decoded[1];

  auto const expected_header = nlohmann::json{
      {"alg", "HS256"}, {"typ", "JWT"}, {"kid", "test-private-key-id"}};
  EXPECT_EQ(expected_header, header) << "header=" << header;

  auto const iat =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
  auto const exp = iat + std::chrono::hours(1);
  auto const expected_payload = nlohmann::json{
      {"iss", info.client_email}, {"sub", info.client_email},
      {"aud", info.audience},     {"iat", iat.count()},
      {"exp", exp.count()},
  };
  EXPECT_EQ(expected_payload, payload) << "payload=" << payload;
}

TEST(SelfSigningServiceAccountCredentials, Basic) {
  auto const info = SelfSigningServiceAccountCredentialsInfo{
      /*.client_email*/ "--invalid--@developer.gserviceaccount.com",
      /*.private_key_id*/ "test-private-key-id",
      /*.private_key*/ testing::kWellFormatedKey,
      /*.audience*/ "test-audience",
  };
  auto actual = std::make_shared<SelfSigningServiceAccountCredentials>(info);
  EXPECT_EQ(info.client_email, actual->AccountEmail());
  EXPECT_EQ(info.private_key_id, actual->KeyId());
  auto const blob = std::string{"The quick brown fox jumps over the lazy dog"};
  auto const signed_blob_failure =
      actual->SignBlob(SigningAccount("mismatch-client-email"), blob);
  EXPECT_THAT(signed_blob_failure, StatusIs(StatusCode::kInvalidArgument));

  auto const signed_blob =
      actual->SignBlob(SigningAccount(info.client_email), blob);
  EXPECT_THAT(signed_blob, IsOk());

  auto const header = actual->AuthorizationHeader();
  ASSERT_THAT(header, IsOk());
  EXPECT_THAT(*header, StartsWith("Authorization: Bearer "));

  auto const h2 = actual->AuthorizationHeader();
  ASSERT_THAT(h2, IsOk());
  EXPECT_EQ(*header, *h2);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
