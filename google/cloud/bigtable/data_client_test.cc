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

#include "google/cloud/bigtable/data_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

TEST(DataClientTest, Default) {
  auto data_client =
      CreateDefaultDataClient("test-project", "test-instance",
                              ClientOptions().set_connection_pool_size(1));
  ASSERT_TRUE(data_client);
  EXPECT_EQ("test-project", data_client->project_id());
  EXPECT_EQ("test-instance", data_client->instance_id());

  auto channel0 = data_client->Channel();
  EXPECT_TRUE(channel0);

  auto channel1 = data_client->Channel();
  EXPECT_EQ(channel0.get(), channel1.get());

  data_client->reset();
  channel1 = data_client->Channel();
  EXPECT_TRUE(channel1);
  EXPECT_NE(channel0.get(), channel1.get());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
