// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bigtable/client/metadata_update_policy.h"
#include <gtest/gtest.h>

std::string const kInstanceName = "projects/foo-project/instances/bar-instance";
std::string const kTableId = "baz-table";
std::string const kTableName =
    "projects/foo-project/instances/bar-instance/tables/baz-table";

/// @test A cloning test for normal construction of metadata .
TEST(MetadataUpdatePolicy, SimpleDefault) {
  auto const x_google_request_params = "parent=" + kInstanceName;
  bigtable::MetadataUpdatePolicy created(kInstanceName,
                                         bigtable::MetadataParamTypes::PARENT);
  EXPECT_EQ(x_google_request_params, created.x_google_request_params().second);
}

/// @test A test for lazy behaviour of metadata .
TEST(MetadataUpdatePolicy, SimpleLazy) {
  auto const x_google_request_params = "name=" + kTableName;
  bigtable::MetadataUpdatePolicy created(
      kInstanceName, bigtable::MetadataParamTypes::NAME, kTableId);
  EXPECT_EQ(x_google_request_params, created.x_google_request_params().second);
}
