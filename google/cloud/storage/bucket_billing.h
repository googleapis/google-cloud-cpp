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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_BILLING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_BILLING_H

#include "google/cloud/storage/version.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The billing configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/requester-pays for general
 *     information on "Requester Pays" billing.
 */
struct BucketBilling {
  BucketBilling() = default;
  // NOLINTNEXTLINE(google-explicit-constructor)
  BucketBilling(bool v) : requester_pays(v) {}

  bool requester_pays{false};
};

inline bool operator==(BucketBilling const& lhs, BucketBilling const& rhs) {
  return lhs.requester_pays == rhs.requester_pays;
}

inline bool operator<(BucketBilling const& lhs, BucketBilling const& rhs) {
  return !lhs.requester_pays && rhs.requester_pays;
}

inline bool operator!=(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketBilling const& lhs, BucketBilling const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_BILLING_H
