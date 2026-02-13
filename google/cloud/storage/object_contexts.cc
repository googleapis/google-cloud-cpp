// Copyright 2026 Google LLC
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

#include "google/cloud/storage/object_contexts.h"
#include "google/cloud/internal/format_time_point.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::ostream& operator<<(std::ostream& os,
                         ObjectCustomContextPayload const& rhs) {
  return os << "ObjectCustomContextPayload={value=" << rhs.value
            << ", create_time="
            << google::cloud::internal::FormatRfc3339(rhs.create_time)
            << ", update_time="
            << google::cloud::internal::FormatRfc3339(rhs.update_time) << "}";
}

std::ostream& operator<<(std::ostream& os, ObjectContexts const& rhs) {
  os << "ObjectContexts={custom={";
  if (rhs.has_custom()) {
    char const* sep = "";
    for (auto const& kv : rhs.custom()) {
      os << sep << kv.first << "=";
      if (kv.second.has_value()) {
        os << kv.second.value();
      } else {
        os << "null";
      }
      sep = ",\n";
    }
  } else {
    os << "null";
  }
  return os << "}}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
