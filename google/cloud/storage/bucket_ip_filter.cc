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

#include "google/cloud/storage/bucket_ip_filter.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/ios_flags_saver.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool operator==(BucketIpFilterPublicNetworkSource const& lhs,
                BucketIpFilterPublicNetworkSource const& rhs) {
  return lhs.allowed_ip_cidr_ranges == rhs.allowed_ip_cidr_ranges;
}

bool operator!=(BucketIpFilterPublicNetworkSource const& lhs,
                BucketIpFilterPublicNetworkSource const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os,
                         BucketIpFilterPublicNetworkSource const& rhs) {
  return os << "BucketIpFilterPublicNetworkSource={allowed_ip_cidr_ranges=["
            << absl::StrJoin(rhs.allowed_ip_cidr_ranges, ", ") << "]}";
}

bool operator==(BucketIpFilterVpcNetworkSource const& lhs,
                BucketIpFilterVpcNetworkSource const& rhs) {
  return std::tie(lhs.network, lhs.allowed_ip_cidr_ranges) ==
         std::tie(rhs.network, rhs.allowed_ip_cidr_ranges);
}

bool operator!=(BucketIpFilterVpcNetworkSource const& lhs,
                BucketIpFilterVpcNetworkSource const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os,
                         BucketIpFilterVpcNetworkSource const& rhs) {
  return os << "BucketIpFilterVpcNetworkSource={network=" << rhs.network
            << ", allowed_ip_cidr_ranges=["
            << absl::StrJoin(rhs.allowed_ip_cidr_ranges, ", ") << "]}";
}

bool operator==(BucketIpFilter const& lhs, BucketIpFilter const& rhs) {
  return std::tie(lhs.allow_all_service_agent_access, lhs.allow_cross_org_vpcs,
                  lhs.mode, lhs.public_network_source,
                  lhs.vpc_network_sources) ==
         std::tie(rhs.allow_all_service_agent_access, rhs.allow_cross_org_vpcs,
                  rhs.mode, rhs.public_network_source, rhs.vpc_network_sources);
}

bool operator!=(BucketIpFilter const& lhs, BucketIpFilter const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, BucketIpFilter const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  os << "BucketIpFilter={mode=" << rhs.mode.value_or("");
  if (rhs.allow_all_service_agent_access) {
    os << ", allow_all_service_agent_access=" << std::boolalpha
       << *rhs.allow_all_service_agent_access;
  }
  if (rhs.allow_cross_org_vpcs) {
    os << ", allow_cross_org_vpcs=" << std::boolalpha
       << *rhs.allow_cross_org_vpcs;
  }
  if (rhs.public_network_source) {
    os << ", public_network_source=" << *rhs.public_network_source;
  }
  if (rhs.vpc_network_sources) {
    os << ", vpc_network_sources=["
       << absl::StrJoin(*rhs.vpc_network_sources, ", ", absl::StreamFormatter())
       << "]";
  }
  return os << "}";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
