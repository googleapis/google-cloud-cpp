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
#include "google/cloud/storage/internal/make_jwt_assertion.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/version.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

auto constexpr kExpiration = std::chrono::hours(1);
auto constexpr kExpirationSlack = std::chrono::minutes(1);

StatusOr<std::string> CreateBearerToken(
    SelfSigningServiceAccountCredentialsInfo const& info,
    std::chrono::system_clock::time_point tp) {
  auto const header = nlohmann::json{
      {"alg", "HS256"}, {"typ", "JWT"}, {"kid", info.private_key_id}};
  auto const iat =
      std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
  auto const exp = iat + kExpiration;
  auto const payload = nlohmann::json{
      {"iss", info.client_email}, {"sub", info.client_email},
      {"aud", info.audience},     {"iat", iat.count()},
      {"exp", exp.count()},
  };
  return MakeJWTAssertionNoThrow(header.dump(), payload.dump(),
                                 info.private_key);
}

StatusOr<std::string>
SelfSigningServiceAccountCredentials::AuthorizationHeader() {
  auto const now = std::chrono::system_clock::now();
  std::lock_guard<std::mutex> lk(mu_);
  if (now + kExpirationSlack <= expiration_time_) {
    return authorization_header_;
  }
  auto token = CreateBearerToken(info_, now);
  if (!token) return std::move(token).status();
  expiration_time_ = now + kExpiration;
  authorization_header_ = "Authorization: Bearer " + *std::move(token);
  return authorization_header_;
}

StatusOr<std::vector<std::uint8_t>>
SelfSigningServiceAccountCredentials::SignBlob(
    SigningAccount const& signing_account,
    std::string const& string_to_sign) const {
  if (signing_account.has_value() &&
      signing_account.value() != info_.client_email) {
    return Status(StatusCode::kInvalidArgument,
                  "Cannot sign blobs for " + signing_account.value());
  }
  return internal::SignStringWithPem(string_to_sign, info_.private_key,
                                     oauth2::JwtSigningAlgorithms::RS256);
}

std::string SelfSigningServiceAccountCredentials::AccountEmail() const {
  return info_.client_email;
}

std::string SelfSigningServiceAccountCredentials::KeyId() const {
  return info_.private_key_id;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
