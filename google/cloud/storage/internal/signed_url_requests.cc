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
#include "google/cloud/storage/internal/format_time_point.h"
#include "google/cloud/storage/internal/sha256_hash.h"
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

std::chrono::system_clock::time_point
V2SignUrlRequest::DefaultExpirationTime() {
  return std::chrono::system_clock::now() + std::chrono::hours(7 * 24);
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
    SetOption(AddExtensionHeader("host", "storage.googleapis.com"));
  }
}

std::string V4SignUrlRequest::CanonicalQueryString(
    std::string const& client_id) const {
  CurlHandle curl;
  auto parameters = CanonicalQueryParameters(client_id);
  return QueryStringFromParameters(curl, parameters);
}

std::string V4SignUrlRequest::CanonicalRequest(
    std::string const& client_id) const {
  std::ostringstream os;

  os << verb() << "\n";
  CurlHandle curl;
  os << '/' << bucket_name();
  if (!object_name().empty()) {
    os << '/' << curl.MakeEscapedString(object_name()).get();
  }
  if (!sub_resource().empty()) {
    os << '?' << curl.MakeEscapedString(sub_resource()).get();
  }
  os << "\n";

  // Query parameters.
  auto parameters = common_request_.query_parameters();
  auto canonical_parameters = CanonicalQueryParameters(client_id);
  // No .merge() until C++17, blegh.
  parameters.insert(canonical_parameters.begin(), canonical_parameters.end());
  os << QueryStringFromParameters(curl, parameters) << "\n";

  // Headers
  for (auto&& kv : common_request_.extension_headers()) {
    os << kv.first << ":" << TrimHeaderValue(kv.second) << "\n";
  }
  os << "\n" << SignedHeaders() << "\nUNSIGNED-PAYLOAD";

  return std::move(os).str();
}

std::string V4SignUrlRequest::StringToSign(std::string const& client_id) const {
  return "GOOG4-RSA-SHA256\n" + FormatV4SignedUrlTimestamp(timestamp_) + "\n" +
         Scope() + "\n" + CanonicalRequestHash(client_id);
}

std::chrono::system_clock::time_point V4SignUrlRequest::DefaultTimestamp() {
  return std::chrono::system_clock::now();
}

std::chrono::seconds V4SignUrlRequest::DefaultExpires() {
  return std::chrono::seconds(7 * std::chrono::hours(24));
}

std::string V4SignUrlRequest::CanonicalRequestHash(
    std::string const& client_id) const {
  return HexEncode(Sha256Hash(CanonicalRequest(client_id)));
}

std::string V4SignUrlRequest::Scope() const {
  return FormatV4SignedUrlScope(timestamp_) + "/auto/storage/goog4_request";
}

std::multimap<std::string, std::string>
V4SignUrlRequest::CanonicalQueryParameters(std::string const& client_id) const {
  return {
      {"X-Goog-Algorithm", "GOOG4-RSA-SHA256"},
      {"X-Goog-Credential", client_id + "/" + Scope()},
      {"X-Goog-Date", FormatV4SignedUrlTimestamp(timestamp_)},
      {"X-Goog-Expires", std::to_string(expires_.count())},
      {"X-Goog-SignedHeaders", SignedHeaders()},
  };
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
