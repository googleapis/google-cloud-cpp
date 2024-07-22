// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/bucket_name.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include <ostream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kPrefix = "projects/_/buckets/";

}

BucketName::BucketName(std::string name) : name_(std::move(name)) {}

std::string BucketName::FullName() const { return kPrefix + name_; }

bool operator==(BucketName const& a, BucketName const& b) {
  return a.name_ == b.name_;
}

bool operator!=(BucketName const& a, BucketName const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, BucketName const& p) {
  return os << p.FullName();
}

StatusOr<BucketName> MakeBucketName(absl::string_view full_name) {
  if (!absl::ConsumePrefix(&full_name, kPrefix)) {
    return internal::InvalidArgumentError(
        absl::StrCat("missing prefix (", kPrefix,
                     ") in bucket name: ", full_name),
        GCP_ERROR_INFO());
  }
  return BucketName(std::string(full_name));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
