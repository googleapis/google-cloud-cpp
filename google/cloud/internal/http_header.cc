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

#include "google/cloud/internal/http_header.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

HttpHeader::HttpHeader(std::string key) : key_(std::move(key)) {}

HttpHeader::HttpHeader(std::string key, std::string value)
    : key_(std::move(key)), values_({std::move(value)}) {}

HttpHeader::HttpHeader(std::string key, std::vector<std::string> values)
    : key_(std::move(key)), values_(std::move(values)) {}

HttpHeader::HttpHeader(std::string key,
                       std::initializer_list<char const*> values)
    : key_(std::move(key)) {
  for (auto&& v : values) values_.emplace_back(v);
}

bool operator==(HttpHeader const& lhs, HttpHeader const& rhs) {
  return absl::AsciiStrToLower(lhs.key_) == absl::AsciiStrToLower(rhs.key_) &&
         lhs.values_ == rhs.values_;
}

bool operator<(HttpHeader const& lhs, HttpHeader const& rhs) {
  return absl::AsciiStrToLower(lhs.key_) < absl::AsciiStrToLower(rhs.key_);
}

bool HttpHeader::IsSameKey(std::string const& key) const {
  return absl::AsciiStrToLower(key_) == absl::AsciiStrToLower(key);
}

bool HttpHeader::IsSameKey(HttpHeader const& other) const {
  return IsSameKey(other.key_);
}

HttpHeader::operator std::string() const {
  if (key_.empty()) return {};
  if (values_.empty()) return absl::StrCat(key_, ":");
  return absl::StrCat(key_, ": ", absl::StrJoin(values_, ","));
}

std::string HttpHeader::DebugString() const {
  if (key_.empty()) return {};
  if (values_.empty()) return absl::StrCat(key_, ":");
  return absl::StrCat(
      key_, ": ",
      absl::StrJoin(values_, ",", [](std::string* out, std::string const& v) {
        absl::StrAppend(out, v.substr(0, 10));
      }));
}

HttpHeader& HttpHeader::MergeHeader(HttpHeader const& other) {
  if (!IsSameKey(other)) return *this;
  values_.insert(values_.end(), other.values_.begin(), other.values_.end());
  return *this;
}

HttpHeader& HttpHeader::MergeHeader(HttpHeader&& other) {
  if (!IsSameKey(other)) return *this;
  values_.insert(values_.end(), std::make_move_iterator(other.values_.begin()),
                 std::make_move_iterator(other.values_.end()));
  return *this;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
