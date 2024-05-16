// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/storage/internal/base64.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/make_status.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/str_format.h"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::InvalidArgumentError;

nlohmann::json TransformConditions(
    std::vector<PolicyDocumentCondition> const& conditions) {
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

// https://en.wikipedia.org/wiki/UTF-8

// The masks for 1-byte, 2-byte, 3-byte and 4-byte UTF-8 encodings. The bits
// that are *set* in these masks are used to extract the marker bits, i.e.,
// those indicating the length of the encoding. Negating the mask let's you
// extract the value bits.
auto constexpr kMask1 = 0b1000'0000U;
auto constexpr kMask2 = 0b1110'0000U;
auto constexpr kMask3 = 0b1111'0000U;
auto constexpr kMask4 = 0b1111'1000U;

// The mask for the trailing bytes.
auto constexpr kMaskTrail = 0b1100'0000U;

inline bool IsEncoded(char c, std::int32_t mask) {
  return (static_cast<std::uint8_t>(c) & mask) == ((mask - 1) & mask);
}

// REQUIRES: pos < s.size()
// REQUIRES: n > 0
// Note that all call sites are in this file, so it is trivial to verify the
// requirements are satisfied.
Status ValidateUTF8Encoding(absl::string_view s, std::size_t pos,
                            std::size_t n) {
  if (s.size() - pos < n) {
    return InvalidArgumentError(
        absl::StrCat("Expected UTF-8 string, found partial UTF-8 encoding at ",
                     pos, " string=<", s, ">"),
        GCP_ERROR_INFO());
  }
  for (auto i = pos + 1; i != pos + n; ++i) {
    if (IsEncoded(s[i], kMaskTrail)) continue;
    return InvalidArgumentError(
        absl::StrCat(
            "Expected UTF-8 string, found incorrect UTF-8 encoding at ", pos,
            " string=<", s, ">"),
        GCP_ERROR_INFO());
  }
  return Status{};
}

inline std::uint32_t ValueBits(char c, std::uint32_t mask) {
  return static_cast<std::uint8_t>(c) & ~mask;
}

inline std::uint32_t Header2(char c) { return ValueBits(c, kMask2); }
inline std::uint32_t Header3(char c) { return ValueBits(c, kMask3); }
inline std::uint32_t Header4(char c) { return ValueBits(c, kMask4); }
inline std::uint32_t Trailer(char c) { return ValueBits(c, kMaskTrail); }

inline std::uint32_t DecodeUTF8(std::uint32_t e3, std::uint32_t e2,
                                std::uint32_t e1, std::uint32_t e0) {
  return ((((e3 << 6 | e2) << 6) | e1) << 6) | e0;
}

StatusOr<std::string> Escape1(absl::string_view s, std::size_t pos) {
  auto status = ValidateUTF8Encoding(s, pos, 1);
  if (!status.ok()) return status;
  // Some characters need to be escaped.
  switch (s[pos]) {
    case '\b':
      return std::string("\\b");
    case '\f':
      return std::string("\\f");
    case '\n':
      return std::string("\\n");
    case '\r':
      return std::string("\\r");
    case '\t':
      return std::string("\\t");
    case '\v':
      return std::string("\\v");
    default:
      // Having a default case makes clang-tidy happy.
      break;
  }
  return std::string(s.substr(pos, 1));
}

StatusOr<std::string> Escape2(absl::string_view s, std::size_t pos) {
  auto status = ValidateUTF8Encoding(s, pos, 2);
  if (!status.ok()) return status;
  auto const e = s.substr(pos, 2);
  return absl::StrFormat("\\u%04x",
                         DecodeUTF8(0, 0, Header2(e[0]), Trailer(e[1])));
}

StatusOr<std::string> Escape3(absl::string_view s, std::size_t pos) {
  auto status = ValidateUTF8Encoding(s, pos, 3);
  if (!status.ok()) return status;
  auto const e = s.substr(pos, 3);
  return absl::StrFormat(
      "\\u%04x", DecodeUTF8(0, Header3(e[0]), Trailer(e[1]), Trailer(e[2])));
}

StatusOr<std::string> Escape4(absl::string_view s, std::size_t pos) {
  auto status = ValidateUTF8Encoding(s, pos, 4);
  if (!status.ok()) return status;
  auto const e = s.substr(pos, 4);
  auto codepoint =
      DecodeUTF8(Header4(e[0]), Trailer(e[1]), Trailer(e[2]), Trailer(e[3]));
  if (codepoint <= 0xFFFFU) return absl::StrFormat("\\u%04x", codepoint);
  return absl::StrFormat("\\U%08x", codepoint);
}

StatusOr<std::string> EscapeUTF8(absl::string_view s) {
  using Encoder =
      absl::FunctionRef<StatusOr<std::string>(absl::string_view, std::size_t)>;
  struct {
    std::uint32_t mask;
    Encoder encode;
  } const encodings[] = {
      {kMask1, Escape1},
      {kMask2, Escape2},
      {kMask3, Escape3},
      {kMask4, Escape4},
  };
  // Iterate over all the bytes in the input string. Interpreting each UTF-8
  // sequence as needed.
  std::string result;
  std::size_t pos = 0;
  while (pos != s.size()) {
    // Test if s[pos] is a 1, 2, 3 or 4 byte UTF-8 codepoint. If it is matched
    // add the escaped characters to `result`. If it is not matched return an
    // error.
    bool matched = false;
    std::size_t n = 1;
    for (auto const& e : encodings) {
      if (IsEncoded(s[pos], e.mask)) {
        // The encoder will return an error if the encoding is too short or
        // otherwise invalid.
        auto r = e.encode(s, pos);
        if (!r) return r;
        result += *r;
        matched = true;
        break;
      }
      ++n;
    }
    if (!matched) {
      return InvalidArgumentError(
          absl::StrCat("Expected UTF-8 string, found non-UTF-8 character (",
                       static_cast<int>(s[pos]), ") at ", pos, " string=<", s,
                       ">"),
          GCP_ERROR_INFO());
    }
    // Skip all the bytes in the UTF-8 character.
    pos += n;
  }
  return result;
}

}  // namespace

StatusOr<std::string> PostPolicyV4Escape(std::string const& utf8_bytes) {
  return EscapeUTF8(utf8_bytes);
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
  extension_fields_.emplace_back(std::move(o.value().first),
                                 std::move(o.value().second));
}

void PolicyDocumentV4Request::SetOption(PredefinedAcl const& o) {
  if (!o.has_value()) {
    return;
  }
  extension_fields_.emplace_back("acl", o.HeaderName());
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
    return scheme_ + "://" + policy_document().bucket + "." + host_ + "/";
  }
  return scheme_ + "://" + host_ + "/" + policy_document().bucket + "/";
}

std::string PolicyDocumentV4Request::Credentials() const {
  return signing_email_ + "/" +
         google::cloud::internal::FormatV4SignedUrlScope(document_.timestamp) +
         "/auto/storage/goog4_request";
}

std::vector<PolicyDocumentCondition> PolicyDocumentV4Request::GetAllConditions()
    const {
  std::vector<PolicyDocumentCondition> conditions;
  conditions.reserve(extension_fields_.size());
  std::transform(extension_fields_.begin(), extension_fields_.end(),
                 std::back_inserter(conditions), [](auto const& f) {
                   return PolicyDocumentCondition({f.first, f.second});
                 });
  std::sort(conditions.begin(), conditions.end());
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
