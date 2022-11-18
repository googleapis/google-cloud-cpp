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
#include "google/cloud/internal/external_account_parsing.h"
#include "google/cloud/internal/external_account_source_format.h"
#include "google/cloud/internal/make_status.h"
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Represent the headers in the credentials configuration file.
using Headers = std::map<std::string, std::string>;

bool IsSuccess(rest_internal::HttpStatusCode code) {
  return rest_internal::HttpStatusCode::kMinSuccess <= code &&
         code <= rest_internal::kMinRedirects;
}

Status DecorateHttpError(Status const& status,
                         internal::ErrorContext const& ec) {
  auto builder = GCP_ERROR_INFO().WithContext(ec).WithReason("HTTP REQUEST");
  for (auto const& kv : status.error_info().metadata()) {
    builder.WithMetadata(kv.first, kv.second);
  }
  return Status(status.code(), status.message(),
                std::move(builder).Build(status.code()));
}

StatusOr<std::string> FetchContents(HttpClientFactory const& client_factory,
                                    std::string const& url,
                                    Headers const& headers,
                                    internal::ErrorContext const& ec) {
  auto client = client_factory();
  auto request = rest_internal::RestRequest(url);
  for (auto const& h : headers) request.AddHeader(h.first, h.second);
  auto status = client->Get(request);
  if (!status) return DecorateHttpError(std::move(status).status(), ec);
  auto response = *std::move(status);
  if (!IsSuccess(response->StatusCode())) {
    return DecorateHttpError(rest_internal::AsStatus(std::move(*response)), ec);
  }
  auto payload = std::move(*response).ExtractPayload();
  return rest_internal::ReadAll(std::move(payload));
}

StatusOr<internal::SubjectToken> FetchTokenText(
    HttpClientFactory const& client_factory, std::string const& url,
    Headers const& headers, internal::ErrorContext const& ec) {
  auto contents = FetchContents(client_factory, url, headers, ec);
  if (!contents) return std::move(contents).status();
  return internal::SubjectToken{*std::move(contents)};
}

StatusOr<internal::SubjectToken> FetchTokenJson(
    HttpClientFactory const& client_factory, std::string const& url,
    Headers const& headers, std::string const& field_name,
    internal::ErrorContext const& ec) {
  auto contents = FetchContents(client_factory, url, headers, ec);
  if (!contents) return std::move(contents).status();
  auto error_details = [&](std::string const& msg) {
    return msg + " in JSON object retrieved from `" + url +
           "`, with subject_token_field `" + field_name + "`";
  };

  auto json = nlohmann::json::parse(*contents, nullptr, false);
  if (!json.is_object()) {
    return InvalidArgumentError(error_details("parse error"),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  auto it = json.find(field_name);
  if (it == json.end()) {
    return InvalidArgumentError(error_details("subject token field not found"),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  if (!it->is_string()) {
    return InvalidArgumentError(error_details("invalid type for token field"),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  return internal::SubjectToken{it->get<std::string>()};
}

StatusOr<Headers> ParseHeaders(nlohmann::json const& credentials_source,
                               internal::ErrorContext const& ec) {
  auto h = credentials_source.find("headers");
  if (h == credentials_source.end()) return Headers{};
  if (!h->is_object()) {
    return InvalidArgumentError(
        "invalid type for `headers` field in `credentials_source`",
        GCP_ERROR_INFO().WithContext(ec));
  }
  Headers headers;
  for (auto const& e : h->items()) {
    if (!e.value().is_string()) {
      return InvalidArgumentError("invalid type for `" + e.key() +
                                      "` field in `credentials_source.headers`",
                                  GCP_ERROR_INFO().WithContext(ec));
    }
    headers.emplace(e.key(), e.value().get<std::string>());
  }
  return headers;
}

}  // namespace

StatusOr<ExternalAccountTokenSource> MakeExternalAccountTokenSourceUrl(
    nlohmann::json const& credentials_source, HttpClientFactory client_factory,
    internal::ErrorContext const& ec) {
  auto url =
      ValidateStringField(credentials_source, "url", "credentials_source", ec);
  if (!url) return std::move(url).status();
  auto context = ec;
  context.emplace_back("credentials_source.type", "url");
  context.emplace_back("credentials_source.url.url", *url);

  auto format = ParseExternalAccountSourceFormat(credentials_source, context);
  if (!format) return std::move(format).status();
  auto headers = ParseHeaders(credentials_source, context);
  if (!headers) return std::move(headers).status();

  if (format->type == "text") {
    context.emplace_back("credentials_source.url.type", "text");
    return ExternalAccountTokenSource{
        [cf = std::move(client_factory), url = *std::move(url),
         headers = *std::move(headers), ec = std::move(context)](
            Options const&) { return FetchTokenText(cf, url, headers, ec); }};
  }
  context.emplace_back("credentials_source.url.type", "json");
  context.emplace_back("credentials_source.url.subject_token_field_name",
                       format->subject_token_field_name);
  return ExternalAccountTokenSource{
      [cf = std::move(client_factory), url = *std::move(url),
       headers = *std::move(headers),
       fn = std::move(format->subject_token_field_name),
       ec = std::move(context)](Options const&) {
        return FetchTokenJson(cf, url, headers, fn, ec);
      }};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
