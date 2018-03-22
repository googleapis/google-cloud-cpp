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

#include <gmock/gmock.h>

#include "bigtable/client/testing/table_test_fixture.h"

namespace btproto = ::google::bigtable::v2;

/// Define helper types and functions for this test.
namespace {
class TableReadModifyWriteTest : public bigtable::testing::TableTestFixture {};
}  // anonymous namespace

using namespace testing;

auto create_rules_lambda = [](std::string row_key,
                              bigtable::ReadModifyWriteRule rule) {
  return [row_key, rule](grpc::ClientContext* ctx,
                         btproto::ReadModifyWriteRowRequest const& request,
                         btproto::ReadModifyWriteRowResponse* response) {

    EXPECT_EQ("row-key", request.row_key());
    auto& row = *response->mutable_row();
    row.set_key("lambda-row-key");
    auto& family = *row.add_families();
    auto& column_id = *family.add_columns();
    auto& cells = *column_id.add_cells();
    auto rules_list = request.rules();
    for (auto rule : rules_list) {
      family.set_name(rule.family_name());
      column_id.set_qualifier(rule.column_qualifier());
      if (rule.rule_case() == rule.kAppendValue) {
        cells.set_value(cells.value() + rule.append_value());
      }
      if (rule.rule_case() == rule.kIncrementAmount) {
        if (cells.value().empty()) {
          cells.set_value("0");
        }
        std::int64_t new_increment_amt =
            std::stol(cells.value()) + rule.increment_amount();
        cells.set_value(std::to_string(new_increment_amt));
      }
    }

    return grpc::Status::OK;
  };
};

TEST_F(TableReadModifyWriteTest, MultipleAppendValueTest) {
  std::string const row_key = "row-key";
  std::string const family1 = "family1";
  std::string const column_id1 = "colid1";

  auto mock_read_modify_write_row = create_rules_lambda(
      row_key, bigtable::ReadModifyWriteRule::IncrementAmount(
                   family1, column_id1, 1000));

  EXPECT_CALL(*bigtable_stub_, ReadModifyWriteRow(_, _, _))
      .WillOnce(Invoke(mock_read_modify_write_row));

  auto row = table_.ReadModifyWriteRow(
      row_key,
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id1, "value1"),
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id1,
                                                 "-value2"));

  EXPECT_EQ("lambda-row-key", row.row_key());
  EXPECT_EQ("value1-value2", row.cells().at(0).value());
}

TEST_F(TableReadModifyWriteTest, MultipleIncrementAmountTest) {
  std::string const row_key = "row-key";
  std::string const family1 = "family1";
  std::string const family2 = "family2";
  std::string const column_id1 = "colid1";
  std::string const column_id2 = "colid2";

  auto mock_read_modify_write_row = create_rules_lambda(
      row_key, bigtable::ReadModifyWriteRule::IncrementAmount(
                   family1, column_id1, 1000));

  EXPECT_CALL(*bigtable_stub_, ReadModifyWriteRow(_, _, _))
      .WillOnce(Invoke(mock_read_modify_write_row));

  auto row = table_.ReadModifyWriteRow(
      row_key,
      bigtable::ReadModifyWriteRule::IncrementAmount(family1, column_id1, 1000),
      bigtable::ReadModifyWriteRule::IncrementAmount(family1, column_id2, 200),
      bigtable::ReadModifyWriteRule::IncrementAmount(family2, column_id1, 400));

  EXPECT_EQ("lambda-row-key", row.row_key());
  EXPECT_EQ(1600, std::stol(row.cells().at(0).value()));
}
