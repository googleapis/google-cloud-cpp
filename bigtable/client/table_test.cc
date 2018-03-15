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

#include "bigtable/client/table.h"
#include "bigtable/client/testing/table_test_fixture.h"

/// Define types and functions used in the tests.
namespace {
class TableTest : public bigtable::testing::TableTestFixture {};
}  // anonymous namespace

TEST_F(TableTest, ClientProjectId) {
  EXPECT_EQ(kProjectId, client_->project_id());
}

TEST_F(TableTest, ClientInstanceId) {
  EXPECT_EQ(kInstanceId, client_->instance_id());
}

TEST_F(TableTest, StandaloneInstanceName) {
  EXPECT_EQ(kInstanceName, bigtable::InstanceName(client_));
}

TEST_F(TableTest, StandaloneTableName) {
  EXPECT_EQ(kTableName, bigtable::TableName(client_, kTableId));
}

TEST_F(TableTest, TableName) {
  // clang-format: you are drunk, placing this short function in a single line
  // is not nice.
  EXPECT_EQ(kTableName, table_.table_name());
}

TEST_F(TableTest, TableConstructor) {
  std::string const kOtherTableId = "my-table";
  std::string const kOtherTableName =
      bigtable::TableName(client_, kOtherTableId);
  bigtable::Table table(client_, kOtherTableId);
  EXPECT_EQ(kOtherTableName, table.table_name());
}
