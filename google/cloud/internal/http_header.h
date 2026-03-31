// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_HEADER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_HEADER_H

#include "google/cloud/version.h"
#if defined(_WIN64) || defined(__LP64__) || defined(__x86_64__) || \
    defined(__ppc64__)
#include "absl/container/flat_hash_map.h"
#else
#include <unordered_map>
#endif
#include "absl/strings/ascii.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class represents a case-insensitive HTTP header name by storing all
// strings in lower-case.
class HttpHeaderName {
 public:
  HttpHeaderName() = default;
  HttpHeaderName(std::string name)  // NOLINT(google-explicit-constructor)
      : name_(std::move(name)) {
    absl::AsciiStrToLower(&name_);
  }
  HttpHeaderName(std::string_view name)  // NOLINT(google-explicit-constructor)
      : HttpHeaderName(std::string{name}) {}
  HttpHeaderName(char const* name)  // NOLINT(google-explicit-constructor)
      : HttpHeaderName(std::string{name}) {}

  operator std::string() const {  // NOLINT(google-explicit-constructor)
    return name_;
  }
  operator std::string_view() const {  // NOLINT(google-explicit-constructor)
    return name_;
  }
  operator char const*() const {  // NOLINT(google-explicit-constructor)
    return name_.c_str();
  }

  bool empty() const { return name_.empty(); }
  std::string const& name() const { return name_; }

  friend bool operator==(HttpHeaderName const& lhs, HttpHeaderName const& rhs) {
    return lhs.name_ == rhs.name_;
  }
  friend bool operator<(HttpHeaderName const& lhs, HttpHeaderName const& rhs) {
    return lhs.name_ < rhs.name_;
  }
  friend bool operator!=(HttpHeaderName const& lhs, HttpHeaderName const& rhs) {
    return !(lhs == rhs);
  }
  friend bool operator>(HttpHeaderName const& lhs, HttpHeaderName const& rhs) {
    return !(lhs < rhs) && (lhs != rhs);
  }
  friend bool operator>=(HttpHeaderName const& lhs, HttpHeaderName const& rhs) {
    return !(lhs < rhs);
  }
  friend bool operator<=(HttpHeaderName const& lhs, HttpHeaderName const& rhs) {
    return !(lhs > rhs);
  }

 private:
  std::string name_;
};

/**
 * This class represents an HTTP header field.
 */
class HttpHeader {
 public:
  HttpHeader() = default;
  explicit HttpHeader(HttpHeaderName key);
  explicit HttpHeader(std::pair<std::string, std::string> header);
  HttpHeader(HttpHeaderName key, std::string value);
  HttpHeader(HttpHeaderName key, std::initializer_list<char const*> values);
  HttpHeader(HttpHeaderName key, std::vector<std::string> values);

  HttpHeader(HttpHeader&&) = default;
  HttpHeader& operator=(HttpHeader&&) = default;
  HttpHeader(HttpHeader const&) = default;
  HttpHeader& operator=(HttpHeader const&) = default;

  // Equality is determined by a case-insensitive comparison of the key and
  // a case-sensitive comparison of the values. Ordering of the values is
  // significant and two HttpHeaders of the same key must have the same ordering
  // of the same values in order to be considered equal.
  //
  // HTTP/1.1 https://www.rfc-editor.org/rfc/rfc7230#section-3.2.2
  friend bool operator==(HttpHeader const& lhs, HttpHeader const& rhs);
  friend bool operator!=(HttpHeader const& lhs, HttpHeader const& rhs) {
    return !(lhs == rhs);
  }

  // Lower case lexicographic comparison of keys without inspecting the values.
  // This class provides operator< only for sorting purposes.
  friend bool operator<(HttpHeader const& lhs, HttpHeader const& rhs);

  // If the key is empty, the entire HttpHeader is considered empty.
  bool empty() const { return name_.empty(); }

  // Number of values.
  std::size_t size() const { return values_.size(); }

  // Checks to see if the values are empty. Does not inspect the key field.
  bool EmptyValues() const { return values_.empty(); }

  // Performs a case-insensitive comparison of the key.
  bool IsSameKey(HttpHeader const& other) const;
  bool IsSameKey(HttpHeaderName const& name) const;

  std::string name() const { return name_; }
  std::vector<std::string> const& values() const { return values_; }

  // While the RFCs indicate that header keys are case-insensitive, no attempt
  // to convert them to all lowercase is made. Header keys are printed in the
  // case they were constructed with. We rely on libcurl to encode them per the
  // HTTP version used.
  //
  // HTTP/1.1 https://www.rfc-editor.org/rfc/rfc7230#section-3.2
  // HTTP/2 https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2
  explicit operator std::string() const;

  // Formats header as a string, but truncates the value if it is too long, or
  // if it could contain a secret.
  std::string DebugString() const;

  // Merges the values from other into this if the keys are the same.
  HttpHeader& MergeHeader(HttpHeader const& other);
  HttpHeader& MergeHeader(HttpHeader&& other);

  using value_type = std::string;
  using const_iterator = std::vector<value_type>::const_iterator;
  const_iterator begin() const { return values_.begin(); }
  const_iterator end() const { return values_.end(); }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

 private:
  HttpHeaderName name_;
  std::vector<std::string> values_;
};

// Abseil does not guarantee compatibility with 32-bit platforms that they do
// not test with. Support for such platforms is a community effort. Using
// std::unordered_map on 32-bit platforms reduces the likelihood of issues
// arising due to this arrangement.
#if UINTPTR_MAX == UINT64_MAX
// 64-bit architecture
using HttpHeaders = absl::flat_hash_map<HttpHeaderName, HttpHeader>;
#else
// 32-bit architecture
using HttpHeaders = std::unordered_map<HttpHeaderName, HttpHeader>;
#endif  // UINTPTR_MAX == UINT64_MAX

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#if UINTPTR_MAX != UINT64_MAX
// This specialization has to be in the global namespace.
template <>
struct std::hash<google::cloud::rest_internal::HttpHeaderName> {
  std::size_t operator()(
      google::cloud::rest_internal::HttpHeaderName const& k) const noexcept {
    return std::hash<std::string>()(k.name());
  }
};
#endif  // UINTPTR_MAX != UINT64_MAX

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_HEADER_H
