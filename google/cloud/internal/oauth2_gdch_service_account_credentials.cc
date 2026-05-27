// Copyright 2026 Google LLC
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

#include "google/cloud/internal/oauth2_gdch_service_account_credentials.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/make_jwt_assertion.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/parse_service_account_p12_file.h"
#include "google/cloud/internal/rest_response.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <functional>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::MakeJWTAssertionNoThrow;

StatusOr<GDCHServiceAccountCredentials::Info>
GDCHServiceAccountCredentials::Parse(std::string const& content,
                                     std::string const& source) {
  auto credentials = nlohmann::json::parse(content, nullptr, false);
  if (credentials.is_discarded() || !credentials.is_object()) {
    return internal::InvalidArgumentError(absl::StrCat(
        "Invalid GDCHServiceAccountCredentials, parsing failed on ",
        "data loaded from ", source));
  }

  using Validator =
      std::function<Status(absl::string_view name, nlohmann::json::iterator)>;
  using Store = std::function<void(Info&, nlohmann::json::iterator const&)>;
  auto optional_field = [](absl::string_view, nlohmann::json::iterator const&) {
    return Status{};
  };
  auto non_empty_field = [&](absl::string_view name,
                             nlohmann::json::iterator const& l) {
    if (l == credentials.end()) return Status{};
    if (!l->get<std::string>().empty()) return Status{};
    return internal::InvalidArgumentError(
        absl::StrCat("Invalid GDCHServiceAccountCredentials, the ", name,
                     " field is empty on data loaded from ", source));
  };
  auto required_field = [&](absl::string_view name,
                            nlohmann::json::iterator const& l) {
    if (l == credentials.end()) {
      return internal::InvalidArgumentError(
          absl::StrCat("Invalid GDCHServiceAccountCredentials, the ", name,
                       " field is missing on data loaded from ", source));
    }
    return non_empty_field(name, l);
  };

  struct Field {
    std::string name;
    Validator validator;
    Store store;
  };
  std::vector<Field> fields{
      {"project", required_field,
       [](Info& info, nlohmann::json::iterator const& l) {
         info.project_id = l->get<std::string>();
       }},
      {"private_key_id", required_field,
       [&](Info& info, nlohmann::json::iterator const& l) {
         if (l == credentials.end()) return;
         info.private_key_id = l->get<std::string>();
       }},
      {"private_key", required_field,
       [](Info& info, nlohmann::json::iterator const& l) {
         info.private_key = l->get<std::string>();
       }},
      {"name", required_field,
       [&](Info& info, nlohmann::json::iterator const& l) {
         info.service_identity_name = l->get<std::string>();
       }},
      {"ca_cert_path", optional_field,
       [&](Info& info, nlohmann::json::iterator const& l) {
         if (l == credentials.end()) return;
         info.ca_cert_path = l->get<std::string>();
       }},
      {"token_uri", required_field,
       [&](Info& info, nlohmann::json::iterator const& l) {
         info.token_uri = l->get<std::string>();
       }}};

  Info info;
  for (auto& f : fields) {
    auto l = credentials.find(f.name);
    if (l != credentials.end() && !l->is_string()) {
      return internal::InvalidArgumentError(absl::StrCat(
          "Invalid GDCHServiceAccountCredentials, the ", f.name,
          " field is present and is not a string, on data loaded from ",
          source));
    }
    auto status = f.validator(f.name, l);
    if (!status.ok()) return status;
    f.store(info, l);
  }
  return info;
}

std::pair<std::string, std::string>
GDCHServiceAccountCredentials::AssertionComponentsFromInfo(
    Info const& info, std::chrono::system_clock::time_point now) {
  nlohmann::json assertion_header = {{"alg", "ES256"}, {"typ", "JWT"}};
  if (!info.private_key_id.empty()) {
    assertion_header["kid"] = info.private_key_id;
  }

  auto expiration = now + GoogleOAuthAccessTokenLifetime();
  // As much as possible, do the time arithmetic using the std::chrono types.
  // Convert to an integer only when we are dealing with timestamps since the
  // epoch. Note that we cannot use `time_t` directly because that might be a
  // floating point.
  auto const now_from_epoch =
      static_cast<std::intmax_t>(std::chrono::system_clock::to_time_t(now));
  auto const expiration_from_epoch = static_cast<std::intmax_t>(
      std::chrono::system_clock::to_time_t(expiration));
  auto iss_sub_value = absl::StrCat("system:serviceaccount:", info.project_id,
                                    ":", info.service_identity_name);
  nlohmann::json assertion_payload = {
      {"iss", iss_sub_value},
      {"sub", iss_sub_value},
      {"aud", info.token_uri},
      {"iat", now_from_epoch},
      // Resulting access token should expire after one hour.
      {"exp", expiration_from_epoch}};

  // Note: we don't move here as it would prevent copy elision.
  return std::make_pair(assertion_header.dump(), assertion_payload.dump());
}

StatusOr<std::string> GDCHServiceAccountCredentials::MakeJWTAssertion(
    std::string const& header, std::string const& payload,
    std::string const& pem_contents) {
  return internal::MakeJWTAssertionNoThrow(header, payload, pem_contents,
                                           internal::SignatureFormat::kRaw);
}

StatusOr<nlohmann::json> GDCHServiceAccountCredentials::CreateRefreshPayload(
    Info const& info, std::chrono::system_clock::time_point now) {
  auto [header, payload] = AssertionComponentsFromInfo(info, now);
  auto jwt = MakeJWTAssertion(header, payload, info.private_key);
  if (!jwt) return std::move(jwt.status());
  return nlohmann::json{
      {"grant_type", "urn:ietf:params:oauth:token-type:token-exchange"},
      {"audience", info.audience},
      {"requested_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"subject_token", std::move(*jwt)},
      {"subject_token_type", "urn:k8s:params:oauth:token-type:serviceaccount"}};
}

StatusOr<AccessToken> GDCHServiceAccountCredentials::ParseRefreshResponse(
    rest_internal::RestResponse& response,
    std::chrono::system_clock::time_point now) {
  auto status_code = response.StatusCode();
  auto payload = rest_internal::ReadAll(std::move(response).ExtractPayload());
  if (!payload.ok()) return std::move(payload).status();
  auto payload_copy = *payload;
  auto access_token = nlohmann::json::parse(*payload, nullptr, false);
  if (access_token.is_discarded() || access_token.count("access_token") == 0 ||
      access_token.count("expires_in") == 0 ||
      access_token.count("token_type") == 0 ||
      access_token.count("issued_token_type") == 0) {
    auto error_payload =
        payload_copy +
        "Could not find all required fields in response (access_token,"
        " expires_in, token_type, issued_token_type) while trying to obtain an"
        " access token for GDCH service account credentials.";
    return internal::InvalidArgumentError(error_payload, GCP_ERROR_INFO());
  }
  auto expires_in = std::chrono::seconds(access_token.value("expires_in", 0));
  return AccessToken{access_token.value("access_token", ""), now + expires_in};
}

StatusOr<std::unique_ptr<Credentials>>
GDCHServiceAccountCredentials::CreateFromInfo(
    Info info, Options const& options, HttpClientFactory client_factory) {
  // Verify this is usable before returning it.
  auto const tp = std::chrono::system_clock::time_point{};
  auto const [header, payload] = AssertionComponentsFromInfo(info, tp);
  auto jwt = MakeJWTAssertion(header, payload, info.private_key);
  if (!jwt) return jwt.status();
  return StatusOr<std::unique_ptr<Credentials>>(
      std::unique_ptr<GDCHServiceAccountCredentials>(
          new GDCHServiceAccountCredentials(std::move(info), options,
                                            std::move(client_factory))));
}

StatusOr<std::unique_ptr<Credentials>>
GDCHServiceAccountCredentials::CreateFromJsonContents(
    std::string const& contents, std::string const& audience,
    Options const& options, HttpClientFactory client_factory) {
  auto info = Parse(contents, "memory");
  if (!info) return info.status();
  info->audience = audience;
  return CreateFromInfo(*std::move(info), options, std::move(client_factory));
}

StatusOr<std::unique_ptr<Credentials>>
GDCHServiceAccountCredentials::CreateFromFilePath(
    std::string const& path, std::string const& audience,
    Options const& options, HttpClientFactory client_factory) {
  if (path.empty()) {
    return internal::InvalidArgumentError(
        "GOOGLE_APPLICATION_CREDENTIALS env var was empty.", GCP_ERROR_INFO());
  }
  std::ifstream is(path);
  if (!is.is_open()) {
    // We use kUnknown here because we don't know if the file does not exist, or
    // if we were unable to open it for some other reason.
    return internal::UnknownError("Cannot open credentials file " + path,
                                  GCP_ERROR_INFO());
  }
  std::string contents(std::istreambuf_iterator<char>{is}, {});
  return CreateFromJsonContents(std::move(contents), audience, options,
                                std::move(client_factory));
}

GDCHServiceAccountCredentials::GDCHServiceAccountCredentials(
    Info info, Options options, HttpClientFactory client_factory)
    : info_(std::move(info)),
      options_(std::move(options)),
      client_factory_(std::move(client_factory)) {}

StatusOr<AccessToken> GDCHServiceAccountCredentials::GetToken(
    std::chrono::system_clock::time_point tp) {
  Options options = options_;
  if (!info_.ca_cert_path.empty()) {
    options.set<CARootsFilePathOption>(info_.ca_cert_path);
  }
  auto client = client_factory_(std::move(options));
  rest_internal::RestRequest request;
  request.SetPath(info_.token_uri);
  request.AddHeader("Content-Type", "application/json");
  auto payload = CreateRefreshPayload(info_, tp);
  if (!payload) return std::move(payload).status();
  rest_internal::RestContext context;
  auto response = client->Post(context, request, {payload->dump()});
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  return ParseRefreshResponse(**response, tp);
}

StatusOr<std::string> GDCHServiceAccountCredentials::project_id() const {
  return info_.project_id;
}

StatusOr<std::string> GDCHServiceAccountCredentials::project_id(
    Options const&) const {
  // project_id() is stored locally, so any retry options are unnecessary.
  return project_id();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
