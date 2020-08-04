// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/storage/internal/curl_handle.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/internal/format_time_point.h"
#include <nlohmann/json.hpp>
#if GOOGLE_CLOUD_CPP_HAVE_CODECVT
#include <codecvt>
#include <locale>
#endif  // GOOGLE_CLOUD_CPP_HAVE_CODECVT
#include <iomanip>
#include <sstream>

// Some MSVC versions need this.
#if (!_DLL) && (_MSC_VER >= 1900) && (_MSC_VER <= 1911)
std::locale::id std::codecvt<char16_t, char, _Mbstatet>::id;
#endif

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

nlohmann::json TransformConditions(
    std::vector<PolicyDocumentCondition> const& conditions) {
  CurlHandle curl;
  auto res = nlohmann::json::array();
  for (auto const& kv : conditions) {
    std::vector<std::string> elements = kv.elements();
    /**
     * If the elements is of size 2, we've encountered an exact match in
     * object form.  So we create a json object using the first element as the
     * key and the second element as the value.
     */
    if (elements.size() == 2) {
      nlohmann::json object;
      object[elements.at(0)] = elements.at(1);
      res.emplace_back(std::move(object));
    } else {
      if (elements.at(0) == "content-length-range") {
        res.push_back({elements.at(0), std::stol(elements.at(1)),
                       std::stol(elements.at(2))});
      } else {
        res.push_back({elements.at(0), elements.at(1), elements.at(2)});
      }
    }
  }
  return res;
}

/// If c is ASCII escape it and append it to result. Return if it is ASCII.
bool EscapeAsciiChar(std::string& result, char32_t c) {
  switch (c) {
    case '\\':
      result.append("\\\\");
      return true;
    case '\b':
      result.append("\\b");
      return true;
    case '\f':
      result.append("\\f");
      return true;
    case '\n':
      result.append("\\n");
      return true;
    case '\r':
      result.append("\\r");
      return true;
    case '\t':
      result.append("\\t");
      return true;
    case '\v':
      result.append("\\v");
      return true;
  }
  char32_t constexpr kMaxAsciiChar = 127;
  if (c > kMaxAsciiChar) {
    return false;
  }
  result.append(1, static_cast<char>(c));
  return true;
}
}  // namespace

#if GOOGLE_CLOUD_CPP_HAVE_CODECVT && GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
StatusOr<std::string> PostPolicyV4EscapeUTF8(std::string const& utf8_bytes) {
  std::string result;

  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
  std::basic_string<char32_t> utf32;
  try {
    utf32 = conv.from_bytes(utf8_bytes);
  } catch (std::exception const& ex) {
    return Status(StatusCode::kInvalidArgument,
                  std::string("string failed to parse as UTF-8: ") + ex.what());
  }
  utf32 = conv.from_bytes(utf8_bytes);
  for (char32_t c : utf32) {
    bool is_ascii = EscapeAsciiChar(result, c);
    if (!is_ascii) {
      // All unicode characters should be encoded as \udead.
      std::ostringstream os;
      os << "\\u" << std::setw(4) << std::setfill('0') << std::hex
         << static_cast<std::uint32_t>(c);
      result.append(os.str());
    }
  }
  return result;
}
#else   // GOOGLE_CLOUD_CPP_HAVE_CODECVT && GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
StatusOr<std::string> PostPolicyV4EscapeUTF8(std::string const&) {
  return Status(StatusCode::kUnimplemented,
                "Signing POST policies is unavailable with this compiler due "
                "to the lack of `codecvt` header or exception support.");
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_CODECVT

StatusOr<std::string> PostPolicyV4Escape(std::string const& utf8_bytes) {
  std::string result;

  for (char32_t c : utf8_bytes) {
    bool is_ascii = EscapeAsciiChar(result, c);
    if (!is_ascii) {
      // We first try to escape the string assuming it's plain ASCII. If it
      // turns out that there are non-ASCII characters, we fall back to
      // interpreting it as proper UTF-8. We do it because it will be faster in
      // the common case and, more importantly, this allows the common case to
      // work properly on compilers which don't have UTF8 support.
      return PostPolicyV4EscapeUTF8(utf8_bytes);
    }
  }
  return result;
}

std::string PolicyDocumentRequest::StringToSign() const {
  using nlohmann::json;
  auto const document = policy_document();

  json j;
  j["expiration"] = google::cloud::internal::FormatRfc3339(document.expiration);
  j["conditions"] = TransformConditions(document.conditions);

  return std::move(j).dump();
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentRequest const& r) {
  return os << "PolicyDocumentRequest={" << r.StringToSign() << "}";
}

void PolicyDocumentV4Request::SetOption(AddExtensionFieldOption const& o) {
  if (!o.has_value()) {
    return;
  }
  extension_fields_.emplace_back(
      std::make_pair(std::move(o.value().first), std::move(o.value().second)));
}

void PolicyDocumentV4Request::SetOption(PredefinedAcl const& o) {
  if (!o.has_value()) {
    return;
  }
  extension_fields_.emplace_back(std::make_pair("acl", o.HeaderName()));
}

void PolicyDocumentV4Request::SetOption(BucketBoundHostname const& o) {
  if (!o.has_value()) {
    bucket_bound_domain_.reset();
    return;
  }
  bucket_bound_domain_ = o.value();
}

void PolicyDocumentV4Request::SetOption(Scheme const& o) {
  if (!o.has_value()) {
    return;
  }
  scheme_ = o.value();
}

void PolicyDocumentV4Request::SetOption(VirtualHostname const& o) {
  virtual_host_name_ = o.has_value() && o.value();
}

std::chrono::system_clock::time_point PolicyDocumentV4Request::ExpirationDate()
    const {
  return document_.timestamp + document_.expiration;
}

std::string PolicyDocumentV4Request::Url() const {
  if (bucket_bound_domain_) {
    return scheme_ + "://" + *bucket_bound_domain_ + "/";
  }
  if (virtual_host_name_) {
    return scheme_ + "://" + policy_document().bucket +
           ".storage.googleapis.com/";
  }
  return scheme_ + "://storage.googleapis.com/" + policy_document().bucket +
         "/";
}

std::string PolicyDocumentV4Request::Credentials() const {
  return signing_email_ + "/" +
         google::cloud::internal::FormatV4SignedUrlScope(document_.timestamp) +
         "/auto/storage/goog4_request";
}

std::vector<PolicyDocumentCondition> PolicyDocumentV4Request::GetAllConditions()
    const {
  std::vector<PolicyDocumentCondition> conditions;
  for (auto const& field : extension_fields_) {
    conditions.push_back(PolicyDocumentCondition({field.first, field.second}));
  }
  auto const& document = policy_document();
  std::copy(document.conditions.begin(), document.conditions.end(),
            std::back_inserter(conditions));
  conditions.push_back(PolicyDocumentCondition({"bucket", document.bucket}));
  conditions.push_back(PolicyDocumentCondition({"key", document.object}));
  conditions.push_back(PolicyDocumentCondition(
      {"x-goog-date", google::cloud::internal::FormatV4SignedUrlTimestamp(
                          document_.timestamp)}));
  conditions.push_back(
      PolicyDocumentCondition({"x-goog-credential", Credentials()}));
  conditions.push_back(
      PolicyDocumentCondition({"x-goog-algorithm", "GOOG4-RSA-SHA256"}));
  return conditions;
}

std::string PolicyDocumentV4Request::StringToSign() const {
  using nlohmann::json;
  json j;
  j["conditions"] = TransformConditions(GetAllConditions());
  j["expiration"] = google::cloud::internal::FormatRfc3339(ExpirationDate());
  return std::move(j).dump();
}

std::map<std::string, std::string> PolicyDocumentV4Request::RequiredFormFields()
    const {
  std::map<std::string, std::string> res;
  for (auto const& condition : GetAllConditions()) {
    auto const& elements = condition.elements();
    // According to conformance tests, bucket should not be present.
    if (elements.size() == 2 && elements[0] == "bucket") {
      continue;
    }
    if (elements.size() == 2) {
      res[elements[0]] = elements[1];
      continue;
    }
    if (elements.size() == 3 && elements[0] == "eq" && elements[1].size() > 1 &&
        elements[1][0] == '$') {
      res[elements[1].substr(1)] = elements[2];
    }
  }
  return res;
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentV4Request const& r) {
  return os << "PolicyDocumentRequest={" << r.StringToSign() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
