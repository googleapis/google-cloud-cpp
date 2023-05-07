// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(MakeDataConnection, DefaultsOptions) {
  using ::google::cloud::testing_util::ScopedEnvironment;
  ScopedEnvironment emulator_host("BIGTABLE_EMULATOR_HOST", "localhost:1");

  auto conn = MakeDataConnection(
      Options{}
          .set<AppProfileIdOption>("user-supplied")
          // Disable channel refreshing, which is not under test.
          .set<MaxConnectionRefreshOption>(ms(0))
          // Create the minimum number of stubs.
          .set<GrpcNumChannelsOption>(1));
  auto options = conn->options();
  EXPECT_TRUE(options.has<DataRetryPolicyOption>())
      << "Options are not defaulted in MakeDataConnection()";
  EXPECT_EQ(options.get<AppProfileIdOption>(), "user-supplied")
      << "User supplied Options are overridden in MakeDataConnection()";
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
