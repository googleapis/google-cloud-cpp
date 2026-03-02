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
#include "google/cloud/internal/throw_delegate.h"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace internal {

void ValidateObjectContext(std::string const& key, std::string const& value) {
  // Helper lambda to validate shared rules for both keys and values
  auto validate_component = [](std::string const& str, char const* name) {
    // The GCS spec requires each object context key / value to
    // contain 1 - 256 UTF-8 code units. Since each UTF-8 code unit is
    // equivalent to 1 byte, we use std::string::size() to check the
    // number of bytes.
    if (str.empty() || str.size() > 256) {
      google::cloud::internal::ThrowInvalidArgument(
          std::string("Object context ") + name +
          " must be between 1 and 256 UTF-8 code units.");
    }
    if (!std::isalnum(static_cast<unsigned char>(str.front()))) {
      google::cloud::internal::ThrowInvalidArgument(
          std::string("Object context ") + name +
          " must begin with an alphanumeric character.");
    }
    if (str.find_first_of("'\"\\/") != std::string::npos) {
      google::cloud::internal::ThrowInvalidArgument(
          std::string("Object context ") + name +
          " cannot contain ', \", \\, or /.");
    }
  };

  validate_component(key, "keys");
  validate_component(value, "values");

  // Rule specific to keys: Cannot begin with 'goog' (case-insensitive)
  if (key.size() >= 4) {
    std::string prefix = key.substr(0, 4);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (prefix == "goog") {
      google::cloud::internal::ThrowInvalidArgument(
          "Object context keys cannot begin with 'goog' (case-insensitive).");
    }
  }
}

void ValidateObjectContextsAggregate(ObjectContexts const& contexts) {
  // Validate each individual key-value pair and calculate aggregate size.
  for (auto const& kv : contexts.custom()) {
    ValidateObjectContext(kv.first, kv.second.value);
  }

  // Rule: Count limited to 50.
  if (contexts.custom().size() > 50) {
    google::cloud::internal::ThrowInvalidArgument(
        "Object contexts are limited to 50 entries per object.");
  }

  // Note: The API limits the aggregate size to 25 KiB (25,600 bytes).
  // With a max of 50 items and a max of 256 bytes per key and value,
  // the maximum possible size is exactly 50 * (256 + 256) = 25,600 bytes.
  // Therefore, an explicit aggregate size check is mathematically redundant.
}

}  // namespace internal

std::ostream& operator<<(std::ostream& os,
                         ObjectCustomContextPayload const& rhs) {
  return os << "ObjectCustomContextPayload{value=" << rhs.value
            << ", createTime="
            << google::cloud::internal::FormatRfc3339(rhs.create_time)
            << ", updateTime="
            << google::cloud::internal::FormatRfc3339(rhs.update_time) << "}";
}

std::ostream& operator<<(std::ostream& os, ObjectContexts const& rhs) {
  os << "ObjectContexts{custom={";
  char const* sep = "";
  for (auto const& kv : rhs.custom()) {
    os << sep << kv.first << "=" << kv.second;
    sep = ", ";
  }

  return os << "}}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
