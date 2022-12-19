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
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/external_account_source_format.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/json_parsing.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/internal/sha256_hmac.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <chrono>

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

auto constexpr kMetadataTokenTtlHeader = "X-aws-ec2-metadata-token-ttl-seconds";
auto constexpr kDefaultMetadataTokenTtl = std::chrono::seconds(900);
auto constexpr kMetadataTokenHeader = "X-aws-ec2-metadata-token";

using ::google::cloud::internal::InvalidArgumentError;

StatusOr<std::string> GetMetadata(std::string path,
                                  std::string const& session_token,
                                  HttpClientFactory const& client_factory,
                                  Options const& opts) {
  auto client = client_factory(opts);
  auto request = rest_internal::RestRequest().SetPath(std::move(path));
  if (!session_token.empty()) {
    request.AddHeader(kMetadataTokenHeader, session_token);
  }
  auto response = client->Get(request);
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  return rest_internal::ReadAll(std::move(**response).ExtractPayload());
}

StatusOr<internal::SubjectToken> Source(
    HttpClientFactory const& cf, Options const& opts,
    ExternalAccountTokenSourceAwsInfo const& info, std::string const& audience,
    internal::ErrorContext const& ec) {
  auto token = FetchMetadataToken(info, cf, opts, ec);
  if (!token) return std::move(token).status();
  auto region = FetchRegion(info, *token, cf, opts, ec);
  if (!region) return std::move(region).status();
  auto secrets = FetchSecrets(info, *token, cf, opts, ec);
  if (!secrets) return std::move(secrets).status();
  auto now = std::chrono::system_clock::now();
  auto subject = ComputeSubjectToken(info, *region, *secrets, now, audience);
  return internal::SubjectToken{subject.dump()};
}

}  // namespace

StatusOr<ExternalAccountTokenSource> MakeExternalAccountTokenSourceAws(
    nlohmann::json const& credentials_source, std::string const& audience,
    internal::ErrorContext const& ec) {
  auto info = ParseExternalAccountTokenSourceAws(credentials_source, ec);
  if (!info) return std::move(info).status();
  return ExternalAccountTokenSource{
      [info = *std::move(info), audience, ec](HttpClientFactory const& cf,
                                              Options const& opts) {
        return Source(cf, opts, info, audience, ec);
      }};
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
            " please file a feature request at"
            " https://github.com/googleapis/google-cloud-cpp/issues"),
        GCP_ERROR_INFO().WithContext(ec));
  }
  auto region_url = ValidateStringField(credentials_source, "region_url",
                                        "credentials-source", ec);
  if (!region_url) return std::move(region_url).status();
  auto url = ValidateStringField(credentials_source, "url",
                                 "credentials-source", kDefaultUrl, ec);
  if (!url) return std::move(url).status();
  auto verification_url =
      ValidateStringField(credentials_source, "regional_cred_verification_url",
                          "credentials-source", ec);
  if (!verification_url) return std::move(verification_url).status();
  auto imdsv2 =
      ValidateStringField(credentials_source, "imdsv2_session_token_url",
                          "credentials-source", std::string{}, ec);
  if (!imdsv2) return std::move(imdsv2).status();

  auto invalid_url = [](absl::string_view name, absl::string_view value) {
    return absl::StrCat(
        "the `", name,
        "` field should refer to the AWS metadata service, got=<", value, ">");
  };
  auto targets_metadata = [](absl::string_view url) {
    // We probably need a full URL parser to verify the host part is either
    // `169.254.169.254` or `fd00:ec2::254`.  We just assume there is no
    // `userinfo` component. The AWS documentation makes no reference to it, and
    // the component is deprecated in any case.
    return absl::StartsWith(url, "http://169.254.169.254") ||
           absl::StartsWith(url, "http://[fd00:ec2::254]");
  };
  if (!targets_metadata(*url)) {
    return InvalidArgumentError(invalid_url("url", *url),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  if (!targets_metadata(*region_url)) {
    return InvalidArgumentError(invalid_url("region_url", *region_url),
                                GCP_ERROR_INFO().WithContext(ec));
  }
  if (!imdsv2->empty() && !targets_metadata(*imdsv2)) {
    return InvalidArgumentError(
        invalid_url("imdsv2_session_token_url", *imdsv2),
        GCP_ERROR_INFO().WithContext(ec));
  }

  return ExternalAccountTokenSourceAwsInfo{
      /*environment_id=*/*std::move(environment_id),
      /*region_url=*/*std::move(region_url),
      /*url=*/*std::move(url),
      /*regional_cred_verification_url=*/*std::move(verification_url),
      /*imdsv2_session_token_url=*/*std::move(imdsv2)};
}

StatusOr<std::string> FetchMetadataToken(
    ExternalAccountTokenSourceAwsInfo const& info,
    HttpClientFactory const& client_factory, Options const& opts,
    internal::ErrorContext const& /*ec*/) {
  if (info.imdsv2_session_token_url.empty()) return std::string{};
  auto const ttl =
      std::chrono::duration_cast<std::chrono::seconds>(kDefaultMetadataTokenTtl)
          .count();
  auto request = rest_internal::RestRequest()
                     .SetPath(std::move(info.imdsv2_session_token_url))
                     .AddHeader(kMetadataTokenTtlHeader, std::to_string(ttl));
  auto client = client_factory(opts);
  auto response = client->Put(request, {});
  if (!response) return std::move(response).status();
  if (IsHttpError(**response)) return AsStatus(std::move(**response));
  return rest_internal::ReadAll(std::move(**response).ExtractPayload());
}

StatusOr<std::string> FetchRegion(ExternalAccountTokenSourceAwsInfo const& info,
                                  std::string const& metadata_token,
                                  HttpClientFactory const& cf,
                                  Options const& opts,
                                  internal::ErrorContext const& ec) {
  for (auto const* name : {"AWS_REGION", "AWS_DEFAULT_REGION"}) {
    auto env = internal::GetEnv(name);
    if (env.has_value()) return std::move(*env);
  }

  auto payload = GetMetadata(info.region_url, metadata_token, cf, opts);
  if (!payload) return std::move(payload).status();
  if (payload->empty()) {
    return InvalidArgumentError(
        absl::StrCat("invalid (empty) region returned from ", info.region_url),
        GCP_ERROR_INFO().WithContext(ec));
  }
  // The metadata service returns an availability zone, so we must remove the
  // last character to return the region.
  payload->pop_back();
  return *std::move(payload);
}

StatusOr<ExternalAccountTokenSourceAwsSecrets> FetchSecrets(
    ExternalAccountTokenSourceAwsInfo const& info,
    std::string const& metadata_token, HttpClientFactory const& cf,
    Options const& opts, internal::ErrorContext const& ec) {
  auto access_key_id_env = internal::GetEnv("AWS_ACCESS_KEY_ID");
  auto secret_access_key_env = internal::GetEnv("AWS_SECRET_ACCESS_KEY");
  if (access_key_id_env.has_value() && secret_access_key_env.has_value()) {
    auto session_token_env = internal::GetEnv("AWS_SESSION_TOKEN");
    return ExternalAccountTokenSourceAwsSecrets{
        /*access_key_id=*/std::move(*access_key_id_env),
        /*secret_access_key=*/std::move(*secret_access_key_env),
        /*session_token=*/session_token_env.value_or("")};
  }

  // This code fetches the security credentials from the metadata services in an
  // AWS EC2 instance, i.e., a VM. The requests and responses are documented in:
  //  https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/iam-roles-for-amazon-ec2.html#instance-metadata-security-credentials
  auto role = GetMetadata(info.url, metadata_token, cf, opts);
  if (!role) return std::move(role).status();
  auto path = info.url;
  if (path.back() != '/') path.push_back('/');
  path.append(*role);
  auto secrets = GetMetadata(path, metadata_token, cf, opts);
  if (!secrets) return std::move(secrets).status();
  auto json = nlohmann::json::parse(*secrets, nullptr, false);
  if (!json.is_object()) {
    return InvalidArgumentError(
        absl::StrCat("cannot parse AWS security-credentials metadata as JSON"),
        GCP_ERROR_INFO()
            .WithContext(ec)
            .WithMetadata("aws.role", *role)
            .WithMetadata("aws.metadata.path", path));
  }
  auto name = absl::string_view{"aws-security-credentials-response"};
  auto access_key_id = ValidateStringField(json, "AccessKeyId", name, ec);
  if (!access_key_id) return std::move(access_key_id).status();
  auto secret_access_key =
      ValidateStringField(json, "SecretAccessKey", name, ec);
  if (!secret_access_key) return std::move(secret_access_key).status();
  auto session_token = ValidateStringField(json, "Token", name, ec);
  if (!session_token) return std::move(session_token).status();

  return ExternalAccountTokenSourceAwsSecrets{
      /*access_key_id=*/*std::move(access_key_id),
      /*secret_access_key=*/*std::move(secret_access_key),
      /*session_token=*/*std::move(session_token)};
}

using ::google::cloud::internal::HexEncode;
using ::google::cloud::internal::Sha256Hash;
using ::google::cloud::internal::Sha256Hmac;

nlohmann::json ComputeSubjectToken(
    ExternalAccountTokenSourceAwsInfo const& info, std::string const& region,
    ExternalAccountTokenSourceAwsSecrets const& secrets,
    std::chrono::system_clock::time_point now, std::string const& target,
    bool debug) {
  // We need to compute a signed API request to the `GetCallerIdentity` API in
  // AWS's Security Token Service.  The format for these requests is documented
  // at:
  //    https://docs.aws.amazon.com/general/latest/gr/create-signed-request.html
  // As you can see below, the code consists of computing several strings and
  // then computing their HMAC-SHA256 and SHA256 hashes.  The format for these
  // strings is the most delicate portion of the code.  A single extra space,
  // or an excess trailing newline breaks the signature.
  //
  // The secrets are used as inputs into the final "Signature" field. This
  // signature only validates the request with the given input parameters and
  // timestamps.
  //
  // In almost all cases the URL will be
  //    https://sts.{region}.amazonaws.com?Action=GetCallerIdentity&Version=2011-06-15
  //
  // In fact, that is the documented URL for the `GetCallerIdentity` API, but
  // we need to be prepared for VPC-SC and other environments where the service
  // may have a different name. As usual, we need to use the canonical `Host`
  // header for this service.

  // The info.regional_cred_verification_url is really a template. The {region}
  // tag needs to be replaced with the actual region.
  auto const verification_url = absl::StrReplaceAll(
      info.regional_cred_verification_url, {{"{region}", region}});

  // We need to split the URL into the query and the base.
  std::vector<absl::string_view> verification =
      absl::StrSplit(verification_url, absl::MaxSplits('?', 1));
  auto const canonical_query_string =
      verification.size() == 2
          ? std::string{verification[1]}
          : std::string{"Action=GetCallerIdentity&Version=2011-06-15"};

  auto const host = absl::StrCat("sts.", region, ".amazonaws.com");
  auto const timestamp = internal::FormatV4SignedUrlTimestamp(now);
  auto const signed_headers = std::string{"host;x-amz-date"};
  auto const body = std::string{};
  auto const body_hash = HexEncode(Sha256Hash(body));

  auto const canonical_request = absl::StrCat(  //
      "POST\n",                                 // Method
      "/\n",                                    // CanonicalUri
      canonical_query_string, "\n",             //
      "host:", host, "\n",                      // CanonicalHeaders
      "x-amz-date:", timestamp, "\n",           //
      signed_headers, "\n",                     //
      body_hash                                 //
  );
  auto const canonical_request_hash = HexEncode(Sha256Hash(canonical_request));

  auto const date = internal::FormatV4SignedUrlScope(now);
  auto const string_to_sign = absl::StrCat(      //
      "AWS4-HMAC-SHA256\n",                      //
      timestamp, "\n",                           //
      date, "/", region, "/sts/aws4_request\n",  // scope
      canonical_request_hash                     //
  );
  auto const k1 = Sha256Hmac("AWS4" + secrets.secret_access_key, timestamp);
  auto const k2 = Sha256Hmac(k1, region);
  auto const k3 = Sha256Hmac(k2, std::string{"sts"});
  auto const k4 = Sha256Hmac(k3, std::string{"aws4_request"});
  auto const signature = Sha256Hmac(k4, string_to_sign);
  auto const authorization = absl::StrCat(    //
      "AWS-HMAC-SHA256",                      //
      " Credential=", secrets.access_key_id,  //
      ",SignedHeaders=", signed_headers,      //
      ",Signature=", HexEncode(signature)     //
  );
  std::vector<nlohmann::json> headers({
      {{"key", "x-goog-cloud-target-resource"}, {"value", target}},
      {{"key", "x-amz-date"}, {"value", timestamp}},
      {{"key", "authorization"}, {"value", authorization}},
      {{"key", "host"}, {"value", host}},
  });
  // The session token may be empty, in which case we do not need to include it.
  if (!secrets.session_token.empty()) {
    headers.push_back(
        {{"key", "x-amz-security-token"}, {"value", secrets.session_token}});
  }
  if (debug) {
    return nlohmann::json{
        {"url", verification_url},
        {"headers", headers},
        {"method", "POST"},
        {"body", ""},
        {"body_hash", body_hash},
        {"canonical_request", canonical_request},
        {"canonical_request_hash", canonical_request_hash},
        {"string_to_sign", string_to_sign},
        {"k1", HexEncode(k1)},
        {"k2", HexEncode(k2)},
        {"k3", HexEncode(k3)},
        {"k4", HexEncode(k4)},
        {"signature", HexEncode(signature)},
    };
  }
  return nlohmann::json{
      {"url", verification_url},
      {"headers", headers},
      {"method", "POST"},
      {"body", ""},
  };
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
