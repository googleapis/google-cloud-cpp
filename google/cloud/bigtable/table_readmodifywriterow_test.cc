// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = ::google::bigtable::v2;

using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::IsProtoEqual;

/// Define helper types and functions for this test.
class TableReadModifyWriteTest : public bigtable::testing::TableTestFixture {
 public:
  TableReadModifyWriteTest() : TableTestFixture(CompletionQueue{}) {}
};

auto create_rules_lambda = [](std::string const& expected_request_string,
                              std::string const& generated_response_string) {
  return [expected_request_string, generated_response_string](
             grpc::ClientContext* context,
             btproto::ReadModifyWriteRowRequest const& request,
             btproto::ReadModifyWriteRowResponse* response) {
    EXPECT_STATUS_OK(IsContextMDValid(
        *context, "google.bigtable.v2.Bigtable.ReadModifyWriteRow",
        google::cloud::internal::ApiClientHeader()));
    btproto::ReadModifyWriteRowRequest expected_request;
    EXPECT_TRUE(::google::protobuf::TextFormat::ParseFromString(
        expected_request_string, &expected_request));
    EXPECT_THAT(expected_request, IsProtoEqual(request));

    EXPECT_TRUE(::google::protobuf::TextFormat::ParseFromString(
        generated_response_string, response));

    return grpc::Status::OK;
  };
};

TEST_F(TableReadModifyWriteTest, MultipleAppendValueTest) {
  std::string const row_key = "row-key";
  std::string const family1 = "family1";
  std::string const column_id1 = "colid1";
  std::string const request_text = R"""(
table_name: "projects/foo-project/instances/bar-instance/tables/baz-table"
row_key: "row-key"
rules {
  family_name: "family1"
  column_qualifier: "colid1"
  append_value: "value1"
}
rules {
  family_name: "family1"
  column_qualifier: "colid1"
  append_value: "-value2"
}
)""";

  std::string const response_text = R"""(
row {
  key: "response-row-key"
  families {
    name: "response-family1"
    columns {
      qualifier: "response-colid1"
      cells {
        value: "value1-value2"
      }
    }
  }
}
)""";

  auto mock_read_modify_write_row =
      create_rules_lambda(request_text, response_text);

  EXPECT_CALL(*client_, ReadModifyWriteRow)
      .WillOnce(mock_read_modify_write_row);

  auto row = table_.ReadModifyWriteRow(
      row_key,
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id1, "value1"),
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id1,
                                                 "-value2"));

  ASSERT_STATUS_OK(row);
  EXPECT_EQ("response-row-key", row->row_key());
  EXPECT_EQ("response-family1", row->cells().at(0).family_name());
  EXPECT_EQ("response-colid1", row->cells().at(0).column_qualifier());
  EXPECT_EQ(1, (int)row->cells().size());
  EXPECT_EQ("value1-value2", row->cells().at(0).value());
}

TEST_F(TableReadModifyWriteTest, MultipleIncrementAmountTest) {
  std::string const row_key = "row-key";
  std::string const family1 = "family1";
  std::string const family2 = "family2";
  std::string const column_id1 = "colid1";
  std::string const column_id2 = "colid2";
  std::string const request_text = R"""(
                        table_name: "projects/foo-project/instances/bar-instance/tables/baz-table"
                        row_key: "row-key"
                        rules {
                          family_name: "family1"
                          column_qualifier: "colid1"
                          increment_amount: 1000
                        }
                        rules {
                          family_name: "family1"
                          column_qualifier: "colid2"
                          increment_amount: 200
                        }
                        rules {
                          family_name: "family2"
                          column_qualifier: "colid2"
                          increment_amount: 400
                        }
                        )""";

  std::string const response_text = R"""(
                        row {
                          key: "response-row-key"
                          families {
                            name: "response-family1"
                            columns {
                              qualifier: "response-colid1"
                              cells {
                                value: "1200"
                              }
                            }
                          }
                          families {
                            name: "response-family2"
                            columns {
                              qualifier: "response-colid2"
                              cells {
                                value: "400"
                              }
                            }
                          }
                        }
                        )""";

  auto mock_read_modify_write_row =
      create_rules_lambda(request_text, response_text);

  EXPECT_CALL(*client_, ReadModifyWriteRow)
      .WillOnce(mock_read_modify_write_row);

  auto row = table_.ReadModifyWriteRow(
      row_key,
      bigtable::ReadModifyWriteRule::IncrementAmount(family1, column_id1, 1000),
      bigtable::ReadModifyWriteRule::IncrementAmount(family1, column_id2, 200),
      bigtable::ReadModifyWriteRule::IncrementAmount(family2, column_id2, 400));

  ASSERT_STATUS_OK(row);
  EXPECT_EQ("response-row-key", row->row_key());
  EXPECT_EQ("response-family1", row->cells().at(0).family_name());
  EXPECT_EQ("response-colid1", row->cells().at(0).column_qualifier());
  EXPECT_EQ(2, (int)row->cells().size());
  EXPECT_EQ("1200", row->cells().at(0).value());

  EXPECT_EQ("response-family2", row->cells().at(1).family_name());
  EXPECT_EQ("response-colid2", row->cells().at(1).column_qualifier());
  EXPECT_EQ("400", row->cells().at(1).value());
}

TEST_F(TableReadModifyWriteTest, MultipleMixedRuleTest) {
  std::string const row_key = "row-key";
  std::string const family1 = "family1";
  std::string const family2 = "family2";
  std::string const column_id1 = "colid1";
  std::string const column_id2 = "colid2";
  std::string const request_text = R"""(
                        table_name: "projects/foo-project/instances/bar-instance/tables/baz-table"
                        row_key: "row-key"
                        rules {
                          family_name: "family1"
                          column_qualifier: "colid1"
                          increment_amount: 1000
                        }
                        rules {
                          family_name: "family1"
                          column_qualifier: "colid2"
                          append_value: "value_string"
                        }
                        rules {
                          family_name: "family2"
                          column_qualifier: "colid2"
                          increment_amount: 400
                        }
                        )""";

  std::string const response_text = R"""(
                        row {
                          key: "response-row-key"
                          families {
                            name: "response-family1"
                            columns {
                              qualifier: "response-colid1"
                              cells {
                                value: "1200"
                              }
                            }
                          }
                          families {
                            name: "response-family2"
                            columns {
                              qualifier: "response-colid2"
                              cells {
                                value: "value_string"
                              }
                            }
                          }
                        }
                        )""";

  auto mock_read_modify_write_row =
      create_rules_lambda(request_text, response_text);

  EXPECT_CALL(*client_, ReadModifyWriteRow)
      .WillOnce(mock_read_modify_write_row);

  auto row = table_.ReadModifyWriteRow(
      row_key,
      bigtable::ReadModifyWriteRule::IncrementAmount(family1, column_id1, 1000),
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id2,
                                                 "value_string"),
      bigtable::ReadModifyWriteRule::IncrementAmount(family2, column_id2, 400));

  ASSERT_STATUS_OK(row);
  EXPECT_EQ("response-row-key", row->row_key());
  EXPECT_EQ("response-family1", row->cells().at(0).family_name());
  EXPECT_EQ("response-colid1", row->cells().at(0).column_qualifier());
  EXPECT_EQ(2, (int)row->cells().size());
  EXPECT_EQ("1200", row->cells().at(0).value());

  EXPECT_EQ("response-family2", row->cells().at(1).family_name());
  EXPECT_EQ("response-colid2", row->cells().at(1).column_qualifier());
  EXPECT_EQ("value_string", row->cells().at(1).value());
}

TEST_F(TableReadModifyWriteTest, UnrecoverableFailureTest) {
  std::string const row_key = "row-key";
  std::string const family1 = "family1";
  std::string const column_id1 = "colid1";

  EXPECT_CALL(*client_, ReadModifyWriteRow)
      .WillRepeatedly([](grpc::ClientContext* context,
                         google::bigtable::v2::ReadModifyWriteRowRequest const&,
                         google::bigtable::v2::ReadModifyWriteRowResponse*) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context, "google.bigtable.v2.Bigtable.ReadModifyWriteRow",
            google::cloud::internal::ApiClientHeader()));
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh oh");
      });

  EXPECT_FALSE(table_.ReadModifyWriteRow(
      row_key,
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id1, "value1"),
      bigtable::ReadModifyWriteRule::AppendValue(family1, column_id1,
                                                 "-value2")));
}

}  // anonymous namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
