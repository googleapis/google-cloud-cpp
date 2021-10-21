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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include <algorithm>
#include <iostream>
#include <numeric>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::ostream& operator<<(std::ostream& os, PolicyDocumentCondition const& rhs) {
  return os << "PolicyDocumentCondition=["
            << absl::StrJoin(rhs.elements(), ", ") << "]";
}

std::ostream& operator<<(std::ostream& os, PolicyDocument const& rhs) {
  os << "PolicyDocument={";
  os << "expiration=" << google::cloud::internal::FormatRfc3339(rhs.expiration)
     << ", ";
  os << "conditions=[";
  os << absl::StrJoin(rhs.conditions, ", ", absl::StreamFormatter());
  return os << "]}";
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentV4 const& rhs) {
  os << "PolicyDocumentV4={";

  os << "bucket=" << rhs.bucket << ", object=" << rhs.object
     << ", expiration=" << rhs.expiration.count()
     << ", timestamp=" << google::cloud::internal::FormatRfc3339(rhs.timestamp)
     << ", ";
  os << "conditions=[";
  os << absl::StrJoin(rhs.conditions, ", ", absl::StreamFormatter());
  return os << "]}";
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentResult const& rhs) {
  return os << "PolicyDocumentResult={"
            << "access_id=" << rhs.access_id << ", expiration="
            << google::cloud::internal::FormatRfc3339(rhs.expiration)
            << ", policy=" << rhs.policy << ", signature=" << rhs.signature
            << "}";
}

std::string FormatDateForForm(PolicyDocumentV4Result const&) {
  // The V4 signed URL format for timestamps and the format for dates in the V4
  // policy docs are fortunately the same, so we can just call the existing
  // function and truncate the sub-day parts.
  auto constexpr kDateLength = sizeof("YYYYMMDD");
  return google::cloud::internal::FormatV4SignedUrlTimestamp(
             std::chrono::system_clock::now())
      .substr(0, kDateLength);
}

std::ostream& operator<<(std::ostream& os, PolicyDocumentV4Result const& rhs) {
  return os << "PolicyDocumentV4Result={"
            << "url=" << rhs.url << ", access_id=" << rhs.access_id
            << ", expiration="
            << google::cloud::internal::FormatRfc3339(rhs.expiration)
            << ", policy=" << rhs.policy << ", signature=" << rhs.signature
            << ", signing_algorithm=" << rhs.signing_algorithm << "}";
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
