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

#include "google/cloud/internal/external_account_token_source_aws.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/external_account_source_format.h"
#include "google/cloud/internal/json_parsing.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/**
 * The default URL to query AWS metadata.
 *
 * In some scenarios, we may need to contact the AWS metadata service to
 * retrieve the security credentials associated with the VM. The URLs for this
 * purpose are documented at:
 *
 * https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/iam-roles-for-amazon-ec2.html#instance-metadata-security-credentials
 *
 * Note that `169.254.169.254` is a [link-local address], it should not leave
 * the local subnetwork that the host is connected to.
 *
 * [link-local address]: https://en.wikipedia.org/wiki/Link-local_address
 */
auto constexpr kDefaultUrl =
    "http://169.254.169.254/latest/meta-data/iam/security-credentials";

using ::google::cloud::internal::InvalidArgumentError;

StatusOr<internal::SubjectToken> Idmsv2TokenSource(
    ExternalAccountTokenSourceAwsInfo const& info,
    HttpClientFactory const& client_factory, Options const& opts) {
  auto client = client_factory(opts);
  auto request =
      rest_internal::RestRequest().SetPath(info.imdsv2_session_token_url);
  auto response = client->Get(request);
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  auto payload = rest_internal::ReadAll(std::move(**response).ExtractPayload());
  if (!payload) return std::move(payload).status();
  return internal::SubjectToken{*std::move(payload)};
}

}  // namespace

StatusOr<ExternalAccountTokenSource> MakeExternalAccountTokenSourceAws(
    nlohmann::json const& credentials_source,
    internal::ErrorContext const& ec) {
  auto info = ParseExternalAccountTokenSourceAws(credentials_source, ec);
  if (!info) return std::move(info).status();
  if (!info->imdsv2_session_token_url.empty()) {
    return ExternalAccountTokenSource{
        [info = *std::move(info)](HttpClientFactory const& cf,
                                  Options const& opts) {
          return Idmsv2TokenSource(info, cf, opts);
        }};
  }
  return internal::UnimplementedError("WIP", GCP_ERROR_INFO().WithContext(ec));
}

StatusOr<ExternalAccountTokenSourceAwsInfo> ParseExternalAccountTokenSourceAws(
    nlohmann::json const& credentials_source,
    internal::ErrorContext const& ec) {
  auto environment_id = ValidateStringField(
      credentials_source, "environment_id", "credentials-source", ec);
  if (!environment_id) return std::move(environment_id).status();
  if (!absl::StartsWith(*environment_id, "aws")) {
    return InvalidArgumentError("`environment_id` does not start with `aws`",
                                GCP_ERROR_INFO().WithContext(ec));
  }
  if (*environment_id != "aws1") {
    return InvalidArgumentError(
        absl::StrCat(
            "only `environment_id=aws1` is supported, but got environment_id=",
            *environment_id,
            ". Consider updating `google-cloud-cpp`, as a new version may",
            " support this environment. If you find this is not the case,",
            " please file a feature at"
            " https://github.com/googleapis/google-cloud-cpp/issues"),
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto region_url = ValidateStringField(credentials_source, "region_url",
                                        "credentials-source", ec);
  if (!region_url) return std::move(region_url).status();
  auto url = ValidateStringField(credentials_source, "url",
                                 "credentials-source", kDefaultUrl, ec);
  if (!url) return std::move(url).status();
  auto regional_url =
      ValidateStringField(credentials_source, "regional_cred_verification_url",
                          "credentials-source", ec);
  if (!regional_url) return std::move(regional_url).status();
  auto imdsv2 =
      ValidateStringField(credentials_source, "imdsv2_session_token_url",
                          "credentials-source", std::string{}, ec);
  if (!imdsv2) return std::move(imdsv2).status();
  return ExternalAccountTokenSourceAwsInfo{
      /*environment_id=*/*std::move(environment_id),
      /*region_url=*/*std::move(region_url),
      /*url=*/*std::move(url),
      /*regional_cred_verification_url=*/*std::move(regional_url),
      /*imdsv2_session_token_url=*/*std::move(imdsv2)};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
