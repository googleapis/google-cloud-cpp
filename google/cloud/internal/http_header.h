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
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class represents an HTTP header field.
 */
class HttpHeader {
 public:
  HttpHeader() = default;
  explicit HttpHeader(std::string key);
  HttpHeader(std::string key, std::string value);
  HttpHeader(std::string key, std::initializer_list<char const*> values);

  HttpHeader(std::string key, std::vector<std::string> values);

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
  bool empty() const { return key_.empty(); }

  // Checks to see if the values are empty. Does not inspect the key field.
  bool EmptyValues() const { return values_.empty(); }

  // Performs a case-insensitive comparison of the key.
  bool IsSameKey(HttpHeader const& other) const;
  bool IsSameKey(std::string const& key) const;

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

 private:
  std::string key_;
  std::vector<std::string> values_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_HTTP_HEADER_H
