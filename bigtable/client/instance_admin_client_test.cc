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

#include "bigtable/client/instance_admin_client.h"
#include <gmock/gmock.h>

TEST(InstanceAdminClientTest, Default) {
  auto admin_client = bigtable::CreateDefaultInstanceAdminClient(
      "test-project", bigtable::ClientOptions().set_connection_pool_size(1));
  ASSERT_TRUE(admin_client);
  EXPECT_EQ("test-project", admin_client->project());

  auto stub0 = admin_client->Stub();
  EXPECT_TRUE(stub0);

  auto stub1 = admin_client->Stub();
  EXPECT_EQ(stub0.get(), stub1.get());

  admin_client->reset();
  stub1 = admin_client->Stub();
  EXPECT_TRUE(stub1);
  EXPECT_NE(stub0.get(), stub1.get());
}
