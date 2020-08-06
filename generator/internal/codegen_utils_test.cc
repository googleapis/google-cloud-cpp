// Copyright 2020 Google LLC
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

#include "generator/internal/codegen_utils.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

TEST(GeneratedFileSuffix, Success) {
  EXPECT_EQ(".gcpcxx.pb", GeneratedFileSuffix());
}

TEST(LocalInclude, Success) {
  EXPECT_EQ("\"google/cloud/status.h\"", LocalInclude("google/cloud/status.h"));
}

TEST(SystemInclude, Success) { EXPECT_EQ("<vector>", SystemInclude("vector")); }

TEST(CamelCaseToSnakeCase, Success) {
  EXPECT_EQ("foo_bar_ba", CamelCaseToSnakeCase("FooBarBa"));
}

TEST(ServiceNameToFilePath, TrailingServiceInLastComponent) {
  EXPECT_EQ("google/spanner/admin/database/v1/database_admin",
            ServiceNameToFilePath(
                "google.spanner.admin.database.v1.DatabaseAdminService"));
}

TEST(ServiceNameToFilePath, NoTrailingServiceInLastComponent) {
  EXPECT_EQ(
      "google/spanner/admin/database/v1/database_admin",
      ServiceNameToFilePath("google.spanner.admin.database.v1.DatabaseAdmin"));
}

TEST(ServiceNameToFilePath, TrailingServiceInIntermediateComponent) {
  EXPECT_EQ(
      "google/spanner/admin/database_service/v1/database_admin",
      ServiceNameToFilePath(
          "google.spanner.admin.databaseService.v1.DatabaseAdminService"));
}

TEST(ProtoNameToCppName, Success) {
  EXPECT_EQ("::google::spanner::admin::database::v1::Request",
            ProtoNameToCppName("google.spanner.admin.database.v1.Request"));
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
