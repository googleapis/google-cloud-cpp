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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(BucketIpFilterTest, PublicNetworkSource) {
  BucketIpFilterPublicNetworkSource source;
  source.allowed_ip_cidr_ranges.emplace_back("1.2.3.4/32");
  source.allowed_ip_cidr_ranges.emplace_back("5.6.7.8/32");

  BucketIpFilterPublicNetworkSource copy = source;
  EXPECT_EQ(source, copy);

  copy.allowed_ip_cidr_ranges.pop_back();
  EXPECT_NE(source, copy);
}

TEST(BucketIpFilterTest, PublicNetworkSourceOrderMatters) {
  BucketIpFilterPublicNetworkSource const source1{{"1.2.3.4/32", "5.6.7.8/32"}};
  BucketIpFilterPublicNetworkSource const source2{{"5.6.7.8/32", "1.2.3.4/32"}};

  // The two sources have the same elements but in a different order.
  // They should NOT be equal.
  EXPECT_NE(source1, source2);
}

TEST(BucketIpFilterTest, VpcNetworkSource) {
  BucketIpFilterVpcNetworkSource source;
  source.network = "projects/p/global/networks/n";
  source.allowed_ip_cidr_ranges.emplace_back("1.2.3.4/32");
  source.allowed_ip_cidr_ranges.emplace_back("5.6.7.8/32");

  BucketIpFilterVpcNetworkSource copy = source;
  EXPECT_EQ(source, copy);

  copy.network = "changed";
  EXPECT_NE(source, copy);
}

TEST(BucketIpFilterTest, VpcNetworkSourceOrderMatters) {
  BucketIpFilterVpcNetworkSource const source1{"projects/p/global/networks/n",
                                               {"1.2.3.4/32", "5.6.7.8/32"}};
  BucketIpFilterVpcNetworkSource const source2{"projects/p/global/networks/n",
                                               {"5.6.7.8/32", "1.2.3.4/32"}};

  // The two sources have the same elements but in a different order.
  // They should NOT be equal.
  EXPECT_NE(source1, source2);
}

TEST(BucketIpFilterTest, IpFilter) {
  BucketIpFilter filter;
  filter.mode = "Enabled";
  filter.allow_all_service_agent_access = true;
  filter.allow_cross_org_vpcs = true;
  filter.public_network_source =
      BucketIpFilterPublicNetworkSource{{"1.2.3.4/32"}};
  filter.vpc_network_sources =
      absl::make_optional<std::vector<BucketIpFilterVpcNetworkSource>>(
          {BucketIpFilterVpcNetworkSource{"projects/p/global/networks/n",
                                          {"5.6.7.8/32"}},
           BucketIpFilterVpcNetworkSource{"projects/p/global/networks/m",
                                          {"9.0.1.2/32"}}});

  BucketIpFilter copy = filter;
  EXPECT_EQ(filter, copy);

  copy.mode = "Disabled";
  EXPECT_NE(filter, copy);
}

TEST(BucketIpFilterTest, IpFilterOrderMatters) {
  BucketIpFilter filter1;
  filter1.vpc_network_sources =
      absl::make_optional<std::vector<BucketIpFilterVpcNetworkSource>>(
          {BucketIpFilterVpcNetworkSource{"projects/p/global/networks/n",
                                          {"1.2.3.4/32"}},
           BucketIpFilterVpcNetworkSource{"projects/p/global/networks/m",
                                          {"5.6.7.8/32"}}});

  BucketIpFilter filter2;
  filter2.vpc_network_sources =
      absl::make_optional<std::vector<BucketIpFilterVpcNetworkSource>>(
          {BucketIpFilterVpcNetworkSource{"projects/p/global/networks/m",
                                          {"5.6.7.8/32"}},
           BucketIpFilterVpcNetworkSource{"projects/p/global/networks/n",
                                          {"1.2.3.4/32"}}});

  // The two filters have the same elements but in a different order.
  // They should NOT be equal.
  EXPECT_NE(filter1, filter2);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
