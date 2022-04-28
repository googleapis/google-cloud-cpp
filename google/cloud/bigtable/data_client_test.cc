// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/internal/logging_data_client.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(DataClientTest, Default) {
  auto data_client =
      CreateDefaultDataClient("test-project", "test-instance",
                              ClientOptions().set_connection_pool_size(1));
  ASSERT_TRUE(data_client);
  EXPECT_EQ("test-project", data_client->project_id());
  EXPECT_EQ("test-instance", data_client->instance_id());

  auto channel0 = internal::DataClientTester::Channel(data_client);
  EXPECT_TRUE(channel0);

  auto channel1 = internal::DataClientTester::Channel(data_client);
  EXPECT_EQ(channel0.get(), channel1.get());

  internal::DataClientTester::reset(data_client);
  channel1 = internal::DataClientTester::Channel(data_client);
  EXPECT_TRUE(channel1);
  EXPECT_NE(channel0.get(), channel1.get());
}

TEST(DataClientTest, MakeClient) {
  auto data_client = MakeDataClient("test-project", "test-instance",
                                    Options{}.set<GrpcNumChannelsOption>(1));
  ASSERT_TRUE(data_client);
  EXPECT_EQ("test-project", data_client->project_id());
  EXPECT_EQ("test-instance", data_client->instance_id());

  auto channel0 = internal::DataClientTester::Channel(data_client);
  EXPECT_TRUE(channel0);

  auto channel1 = internal::DataClientTester::Channel(data_client);
  EXPECT_EQ(channel0.get(), channel1.get());

  internal::DataClientTester::reset(data_client);
  channel1 = internal::DataClientTester::Channel(data_client);
  EXPECT_TRUE(channel1);
  EXPECT_NE(channel0.get(), channel1.get());
}

TEST(DataClientTest, Logging) {
  testing_util::ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", "rpc");

  auto data_client = MakeDataClient("test-project", "test-instance");
  ASSERT_TRUE(data_client);
  ASSERT_TRUE(
      dynamic_cast<internal::LoggingDataClient const*>(data_client.get()))
      << "Should create LoggingDataClient";
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
