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

#include "google/cloud/storage/policy_document.h"
#include "google/cloud/internal/format_time_point.h"
#include <algorithm>
#include <iostream>
#include <numeric>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
std::string Join(char const* sep, std::vector<std::string> const& list) {
  if (list.empty()) {
    return std::string{};
  }
  return std::accumulate(++list.begin(), list.end(), list.front(),
                         [sep](std::string a, std::string const& b) {
                           a += sep;
                           a += b;
                           return a;
                         });
};
}  // namespace

std::ostream& operator<<(std::ostream& os, PolicyDocumentCondition const& rhs) {
  return os << "PolicyDocumentCondition=[" << Join(", ", rhs.elements()) << "]";
}

std::ostream& operator<<(std::ostream& os, PolicyDocument const& rhs) {
  os << "PolicyDocument={";
  os << "expiration=" << google::cloud::internal::FormatRfc3339(rhs.expiration)
     << ", ";
  os << "conditions=[";
  for (auto i = 0U; i != rhs.conditions.size(); ++i) {
    os << rhs.conditions[i];
    if (i + 1 < rhs.conditions.size()) {
      os << ", ";
    }
  }
  return os << "]}";
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentResult const& rhs) {
  return os << "PolicyDocumentResult={"
            << "access_id=" << rhs.access_id << ", expiration="
            << google::cloud::internal::FormatRfc3339(rhs.expiration)
            << ", policy=" << rhs.policy << ", signature=" << rhs.signature
            << "}";
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
