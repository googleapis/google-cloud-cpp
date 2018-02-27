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

#include "bigtable/client/data_client.h"

#include <gmock/gmock.h>

TEST(DataClientTest, Default) {
  auto data_client = bigtable::CreateDefaultDataClient(
      "test-project", "test-instance",
      bigtable::ClientOptions().set_connection_pool_size(1));
  ASSERT_TRUE(data_client);
  EXPECT_EQ("test-project", data_client->project_id());
  EXPECT_EQ("test-instance", data_client->instance_id());

  auto stub0 = data_client->Stub();
  EXPECT_TRUE(stub0);

  auto stub1 = data_client->Stub();
  EXPECT_EQ(stub0.get(), stub1.get());

  data_client->reset();
  stub1 = data_client->Stub();
  EXPECT_TRUE(stub1);
  EXPECT_NE(stub0.get(), stub1.get());
}
