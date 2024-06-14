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

#include "google/cloud/internal/rest_lro_helpers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;

TEST(ParseComputeOperationInfoTest, Global) {
  auto constexpr kGlobalSelfLink =
      "https://www.googleapis.com/compute/v1"
      "/projects/test-project"
      "/global"
      "/operations/test-operation";

  auto info = ParseComputeOperationInfo(kGlobalSelfLink);
  EXPECT_THAT(info.project, Eq("test-project"));
  EXPECT_THAT(info.region, IsEmpty());
  EXPECT_THAT(info.zone, IsEmpty());
  EXPECT_THAT(info.operation, Eq("test-operation"));
}

TEST(ParseComputeOperationInfoTest, GlobalOrganization) {
  auto constexpr kGlobalOrgSelfLink =
      "https://www.googleapis.com/compute/v1"
      "/projects/test-project"
      "/globalOrganization"
      "/operations/test-operation";

  auto info = ParseComputeOperationInfo(kGlobalOrgSelfLink);
  EXPECT_THAT(info.project, Eq("test-project"));
  EXPECT_THAT(info.region, IsEmpty());
  EXPECT_THAT(info.zone, IsEmpty());
  EXPECT_THAT(info.operation, Eq("test-operation"));
}

TEST(ParseComputeOperationInfoTest, Region) {
  auto constexpr kRegionSelfLink =
      "https://www.googleapis.com/compute/v1"
      "/projects/test-project"
      "/regions/test-region"
      "/operations/test-operation";

  auto info = ParseComputeOperationInfo(kRegionSelfLink);
  EXPECT_THAT(info.project, Eq("test-project"));
  EXPECT_THAT(info.region, Eq("test-region"));
  EXPECT_THAT(info.zone, IsEmpty());
  EXPECT_THAT(info.operation, Eq("test-operation"));
}

TEST(ParseComputeOperationInfoTest, Zone) {
  auto constexpr kZoneSelfLink =
      "https://www.googleapis.com/compute/v1"
      "/projects/test-project"
      "/zones/test-zone"
      "/operations/test-operation";

  auto info = ParseComputeOperationInfo(kZoneSelfLink);
  EXPECT_THAT(info.project, Eq("test-project"));
  EXPECT_THAT(info.region, IsEmpty());
  EXPECT_THAT(info.zone, Eq("test-zone"));
  EXPECT_THAT(info.operation, Eq("test-operation"));
}

TEST(ParseComputeOperationInfoTest, HandlesSelfLinkMissingValues) {
  auto constexpr kMissingOperationValueSelfLink =
      "https://www.googleapis.com/compute/v1"
      "/projects/test-project"
      "/zones/test-zone"
      "/operations";

  auto info = ParseComputeOperationInfo(kMissingOperationValueSelfLink);
  EXPECT_THAT(info.project, Eq("test-project"));
  EXPECT_THAT(info.region, IsEmpty());
  EXPECT_THAT(info.zone, Eq("test-zone"));
  EXPECT_THAT(info.operation, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
