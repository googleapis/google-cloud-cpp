// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/sha256_hash.h"
#include "google/cloud/internal/format_time_point.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

void SignUrlRequestCommon::SetOption(AddExtensionHeaderOption const& o) {
  if (!o.has_value()) {
    return;
  }
  auto kv = o.value();
  // Normalize the header, they are not case sensitive.
  std::transform(kv.first.begin(), kv.first.end(), kv.first.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  auto res = extension_headers_.insert(kv);
  if (!res.second) {
    // The element already exists, we need to append:
    res.first->second.push_back(',');
    res.first->second.append(kv.second);
  }
}

std::vector<std::string> SignUrlRequestCommon::ObjectNameParts() const {
  std::vector<std::string> parts;
  std::istringstream is(object_name());
  std::string part;
  do {
    std::getline(is, part, '/');
    parts.push_back(part);
    is.tellg();
  } while (is);
  return parts;
}

std::vector<std::string> V4SignUrlRequest::ObjectNameParts() const {
  return common_request_.ObjectNameParts();
}

namespace {
std::chrono::hours DefaultV4SignedUrlExpiration() {
  auto constexpr kHoursInDay = 24;
  auto constexpr kDaysInWeek = 7;
  return std::chrono::hours{kDaysInWeek * kHoursInDay};
}
}  // namespace

std::chrono::system_clock::time_point
V2SignUrlRequest::DefaultExpirationTime() {
  return std::chrono::system_clock::now() + DefaultV4SignedUrlExpiration();
}

std::string V2SignUrlRequest::StringToSign() const {
  std::ostringstream os;

  os << verb() << "\n"
     << md5_hash_value_ << "\n"
     << content_type_ << "\n"
     << expiration_time_as_seconds().count() << "\n";

  for (auto const& kv : common_request_.extension_headers()) {
    os << kv.first << ":" << kv.second << "\n";
  }

  CurlHandle curl;
  os << '/' << bucket_name();
  if (!object_name().empty()) {
    os << '/' << curl.MakeEscapedString(object_name()).get();
  }
  char const* sep = "?";
  if (!sub_resource().empty()) {
    os << sep << curl.MakeEscapedString(sub_resource()).get();
    sep = "&";
  }
  for (auto const& kv : common_request_.query_parameters()) {
    os << sep << curl.MakeEscapedString(kv.first).get() << "="
       << curl.MakeEscapedString(kv.second).get();
    sep = "&";
  }

  return std::move(os).str();
}

std::ostream& operator<<(std::ostream& os, V2SignUrlRequest const& r) {
  return os << "SingUrlRequest={" << r.StringToSign() << "}";
}

namespace {
std::string QueryStringFromParameters(
    CurlHandle& curl,
    std::multimap<std::string, std::string> const& parameters) {
  std::string result;
  char const* sep = "";
  for (auto const& qp : parameters) {
    result += sep;
    result += curl.MakeEscapedString(qp.first).get();
    result += '=';
    result += curl.MakeEscapedString(qp.second).get();
    sep = "&";
  }
  return result;
}

std::string TrimHeaderValue(std::string const& value) {
  std::string tmp = value;
  // Heasder values need to be normalized for spaces, whitespaces and tabs
  std::replace_if(
      tmp.begin(), tmp.end(), [](char c) { return std::isspace(c); }, ' ');
  tmp.erase(0, tmp.find_first_not_of(' '));
  tmp = tmp.substr(0, tmp.find_last_not_of(' ') + 1);
  auto end = std::unique(tmp.begin(), tmp.end(),
                         [](char a, char b) { return a == ' ' && b == ' '; });
  tmp.erase(end, tmp.end());
  return tmp;
}
}  // namespace

void V4SignUrlRequest::AddMissingRequiredHeaders() {
  auto const& headers = common_request_.extension_headers();
  if (headers.find("host") == headers.end()) {
    SetOption(AddExtensionHeader("host", Hostname()));
  }
}

void V4SignUrlRequest::SetOption(VirtualHostname const& hostname) {
  virtual_host_name_ = hostname.has_value() && hostname.value();
}

void V4SignUrlRequest::SetOption(BucketBoundHostname const& o) {
  if (!o.has_value()) {
    domain_named_bucket_.reset();
    return;
  }
  domain_named_bucket_ = o.value();
}

void V4SignUrlRequest::SetOption(Scheme const& o) {
  if (!o.has_value()) {
    return;
  }
  scheme_ = o.value();
}

std::string V4SignUrlRequest::CanonicalQueryString(
    std::string const& client_id) const {
  CurlHandle curl;
  // Query parameters.
  auto parameters = AllQueryParameters(client_id);
  return QueryStringFromParameters(curl, parameters);
}

std::string V4SignUrlRequest::CanonicalRequest(
    std::string const& client_id) const {
  std::ostringstream os;

  os << verb() << "\n";
  CurlHandle curl;
  if (!SkipBucketInPath()) {
    os << '/' << bucket_name();
  }
  for (auto& part : ObjectNameParts()) {
    os << '/' << curl.MakeEscapedString(part).get();
  }
  if (!sub_resource().empty()) {
    os << '?' << curl.MakeEscapedString(sub_resource()).get();
  }
  os << "\n";

  // Query parameters.
  auto parameters = AllQueryParameters(client_id);
  os << QueryStringFromParameters(curl, parameters) << "\n";

  // Headers
  for (auto&& kv : common_request_.extension_headers()) {
    os << kv.first << ":" << TrimHeaderValue(kv.second) << "\n";
  }
  os << "\n" << SignedHeaders() << "\n" << PayloadHashValue();

  return std::move(os).str();
}

std::string V4SignUrlRequest::StringToSign(std::string const& client_id) const {
  return "GOOG4-RSA-SHA256\n" +
         google::cloud::internal::FormatV4SignedUrlTimestamp(timestamp_) +
         "\n" + Scope() + "\n" + CanonicalRequestHash(client_id);
}

Status V4SignUrlRequest::Validate() {
  if (virtual_host_name_ && domain_named_bucket_) {
    return Status(StatusCode::kInvalidArgument,
                  "VirtualHostname and BucketBoundHostname cannot be specified "
                  "simultaneously");
  }
  auto const& headers = common_request_.extension_headers();
  auto host_it = headers.find("host");
  if (host_it == headers.end()) {
    return Status();
  }
  if (virtual_host_name_ && host_it->second != Hostname()) {
    return Status(StatusCode::kInvalidArgument,
                  "specified 'host' (" + host_it->second +
                      ") header stands in conflict with "
                      "'VirtualHostname' option.");
  }
  if (domain_named_bucket_ && host_it->second != *domain_named_bucket_) {
    return Status(StatusCode::kInvalidArgument,
                  "specified 'host' (" + host_it->second +
                      ") doesn't match domain specified in the "
                      "'BucketBoundHostname' option (" +
                      *domain_named_bucket_ + ").");
  }
  return Status();
}

std::string V4SignUrlRequest::Hostname() {
  if (virtual_host_name_) {
    return common_request_.bucket_name() + ".storage.googleapis.com";
  }
  if (domain_named_bucket_) {
    return *domain_named_bucket_;
  }
  return "storage.googleapis.com";
}

std::string V4SignUrlRequest::HostnameWithBucket() {
  return scheme_ + "://" + Hostname() +
         (SkipBucketInPath() ? std::string()
                             : ("/" + common_request_.bucket_name()));
}

std::chrono::system_clock::time_point V4SignUrlRequest::DefaultTimestamp() {
  return std::chrono::system_clock::now();
}

std::chrono::seconds V4SignUrlRequest::DefaultExpires() {
  return DefaultV4SignedUrlExpiration();
}

std::string V4SignUrlRequest::CanonicalRequestHash(
    std::string const& client_id) const {
  return HexEncode(Sha256Hash(CanonicalRequest(client_id)));
}

std::string V4SignUrlRequest::Scope() const {
  return google::cloud::internal::FormatV4SignedUrlScope(timestamp_) +
         "/auto/storage/goog4_request";
}

std::multimap<std::string, std::string>
V4SignUrlRequest::CanonicalQueryParameters(std::string const& client_id) const {
  return {
      {"X-Goog-Algorithm", "GOOG4-RSA-SHA256"},
      {"X-Goog-Credential", client_id + "/" + Scope()},
      {"X-Goog-Date",
       google::cloud::internal::FormatV4SignedUrlTimestamp(timestamp_)},
      {"X-Goog-Expires", std::to_string(expires_.count())},
      {"X-Goog-SignedHeaders", SignedHeaders()},
  };
}

std::multimap<std::string, std::string> V4SignUrlRequest::AllQueryParameters(
    std::string const& client_id) const {
  CurlHandle curl;
  // Query parameters.
  auto parameters = common_request_.query_parameters();
  auto canonical_parameters = CanonicalQueryParameters(client_id);
  // No .merge() until C++17, blegh.
  parameters.insert(canonical_parameters.begin(), canonical_parameters.end());
  return parameters;
}

std::string V4SignUrlRequest::SignedHeaders() const {
  std::string result;
  char const* sep = "";
  for (auto&& kv : common_request_.extension_headers()) {
    result += sep;
    result += kv.first;
    sep = ";";
  }
  return result;
}

std::string V4SignUrlRequest::PayloadHashValue() const {
  using value_type = std::map<std::string, std::string>::value_type;
  auto it = std::find_if(common_request_.extension_headers().begin(),
                         common_request_.extension_headers().end(),
                         [](value_type const& entry) {
                           return entry.first == "x-goog-content-sha256" ||
                                  entry.first == "x-amz-content-sha256";
                         });
  if (it != common_request_.extension_headers().end()) {
    return it->second;
  }
  return "UNSIGNED-PAYLOAD";
}

std::ostream& operator<<(std::ostream& os, V4SignUrlRequest const& r) {
  return os << "V4SignUrlRequest={"
            << r.CanonicalRequest("placeholder-client-id") << ","
            << r.StringToSign("placeholder-client-id") << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
