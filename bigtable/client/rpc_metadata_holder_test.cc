// Copyright 2017 Google Inc.
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

#include "bigtable/client/rpc_metadata_holder.h"
#include <gtest/gtest.h>

std::string const kInstanceName = "projects/foo-project/instances/bar-instance";
std::string const kTableId = "baz-table";
std::string const kTableName =
    "projects/foo-project/instances/bar-instance/tables/baz-table";

/// @test A cloning test for normal cloning of metadata .
TEST(RpcMetadataHolder, SimpleClone) {
  auto const x_google_request_params = "parent=" + kInstanceName;
  bigtable::RPCMetadataHolder created(kInstanceName,
                                      bigtable::RPCRequestParamType::kParent);
  std::unique_ptr<bigtable::RPCMetadataHolder> cloned = created.clone();
  EXPECT_EQ(kInstanceName,
            cloned->get_google_cloud_resource_prefix().get_value());
  EXPECT_EQ(x_google_request_params,
            cloned->get_x_google_request_params().get_value());
}

/// @test A cloning test for normal cloning of metadata .
TEST(RpcMetadataHolder, CloneWithModifications) {
  auto const x_google_request_params = "name=" + kTableName;
  bigtable::RPCMetadataHolder created(kInstanceName,
                                      bigtable::RPCRequestParamType::kParent);
  std::unique_ptr<bigtable::RPCMetadataHolder> cloned =
      created.cloneWithModifications(bigtable::RPCRequestParamType::kName,
                                     kTableId);
  EXPECT_EQ(kInstanceName,
            cloned->get_google_cloud_resource_prefix().get_value());
  EXPECT_EQ(x_google_request_params,
            cloned->get_x_google_request_params().get_value());
}
