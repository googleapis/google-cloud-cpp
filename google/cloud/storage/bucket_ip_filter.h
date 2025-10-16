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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_IP_FILTER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_IP_FILTER_H

#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <iosfwd>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Defines a public network source for the bucket IP filter.
 */
struct BucketIpFilterPublicNetworkSource {
  std::vector<std::string> allowed_ip_cidr_ranges;
};

bool operator==(BucketIpFilterPublicNetworkSource const& lhs,
                BucketIpFilterPublicNetworkSource const& rhs);
bool operator!=(BucketIpFilterPublicNetworkSource const& lhs,
                BucketIpFilterPublicNetworkSource const& rhs);

std::ostream& operator<<(std::ostream& os,
                         BucketIpFilterPublicNetworkSource const& rhs);

/**
 * Defines a VPC network source for the bucket IP filter.
 */
struct BucketIpFilterVpcNetworkSource {
  std::string network;
  std::vector<std::string> allowed_ip_cidr_ranges;
};

bool operator==(BucketIpFilterVpcNetworkSource const& lhs,
                BucketIpFilterVpcNetworkSource const& rhs);
bool operator!=(BucketIpFilterVpcNetworkSource const& lhs,
                BucketIpFilterVpcNetworkSource const& rhs);

std::ostream& operator<<(std::ostream& os,
                         BucketIpFilterVpcNetworkSource const& rhs);

/**
 * The IP filtering configuration for a Bucket.
 */
struct BucketIpFilter {
  absl::optional<bool> allow_all_service_agent_access;
  absl::optional<bool> allow_cross_org_vpcs;
  absl::optional<std::string> mode;
  absl::optional<BucketIpFilterPublicNetworkSource> public_network_source;
  absl::optional<std::vector<BucketIpFilterVpcNetworkSource>>
      vpc_network_sources;
};

bool operator==(BucketIpFilter const& lhs, BucketIpFilter const& rhs);
bool operator!=(BucketIpFilter const& lhs, BucketIpFilter const& rhs);

std::ostream& operator<<(std::ostream& os, BucketIpFilter const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_IP_FILTER_H
