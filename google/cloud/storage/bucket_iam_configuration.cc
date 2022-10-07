// Copyright 2022 Google LLC
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

#include "google/cloud/storage/bucket_iam_configuration.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/ios_flags_saver.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::ostream& operator<<(std::ostream& os,
                         UniformBucketLevelAccess const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  return os << "UniformBucketLevelAccess={enabled=" << std::boolalpha
            << rhs.enabled << ", locked_time="
            << google::cloud::internal::FormatRfc3339(rhs.locked_time) << "}";
}

std::ostream& operator<<(std::ostream& os, BucketIamConfiguration const& rhs) {
  os << "BucketIamConfiguration={";
  char const* sep = "";
  if (rhs.public_access_prevention.has_value()) {
    os << sep << "public_access_prevention=" << *rhs.public_access_prevention;
    sep = ", ";
  }
  if (rhs.uniform_bucket_level_access.has_value()) {
    os << sep
       << "uniform_bucket_level_access=" << *rhs.uniform_bucket_level_access;
    return os << "}";
  }
  return os << "}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
