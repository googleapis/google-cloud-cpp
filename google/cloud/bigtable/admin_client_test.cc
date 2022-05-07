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

#include "google/cloud/bigtable/admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

#include "google/cloud/internal/disable_deprecation_warnings.inc"

TEST(AdminClientTest, CreateDefaultClient) {
  auto admin_client = CreateDefaultAdminClient("test-project", {});
  ASSERT_TRUE(admin_client);
  EXPECT_EQ("test-project", admin_client->project());
}

#include "google/cloud/internal/diagnostics_pop.inc"

TEST(AdminClientTest, MakeClient) {
  auto admin_client = MakeAdminClient("test-project");
  ASSERT_TRUE(admin_client);
  EXPECT_EQ("test-project", admin_client->project());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
