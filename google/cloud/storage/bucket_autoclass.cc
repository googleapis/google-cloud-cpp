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

#include "google/cloud/storage/bucket_autoclass.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/ios_flags_saver.h"
#include <iomanip>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::ostream& operator<<(std::ostream& os, BucketAutoclass const& rhs) {
  google::cloud::internal::IosFlagsSaver flags(os);
  return os << "{enabled=" << std::boolalpha << rhs.enabled << ", toggle_time="
            << google::cloud::internal::FormatRfc3339(rhs.toggle_time) << "}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
