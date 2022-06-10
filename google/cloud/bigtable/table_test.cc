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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/mocks/mock_data_connection.h"
#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ms = std::chrono::milliseconds;
using ::google::cloud::bigtable_mocks::internal::MockDataConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Property;

auto const* const kProjectId = "test-project";
auto const* const kInstanceId = "test-instance";
auto const* const kAppProfileId = "test-profile";
auto const* const kTableId = "test-table";
auto const* const kTableName =
    "projects/test-project/instances/test-instance/tables/test-table";

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "fail");
}

bigtable::SingleRowMutation IdempotentMutation() {
  return bigtable::SingleRowMutation(
      "row", {bigtable::SetCell("fam", "col", ms(0), "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation() {
  return bigtable::SingleRowMutation("row",
                                     {bigtable::SetCell("fam", "col", "val")});
}

void CheckFailedMutations(std::vector<FailedMutation> const& actual,
                          std::vector<FailedMutation> const& expected) {
  struct Unroll {
    explicit Unroll(std::vector<FailedMutation> const& failed) {
      for (auto const& f : failed) {
        statuses.push_back(f.status());
        indices.push_back(f.original_index());
      }
    }
    std::vector<Status> statuses;
    std::vector<int> indices;
  };

  auto a = Unroll(actual);
  auto e = Unroll(expected);
  EXPECT_THAT(a.statuses, ElementsAreArray(e.statuses));
  EXPECT_THAT(a.indices, ElementsAreArray(e.indices));
}

RowSet TestRowSet() { return RowSet("r1", "r2"); }

Matcher<RowSet const&> IsTestRowSet() {
  return Property(&RowSet::as_proto,
                  Property(&v2::RowSet::row_keys, ElementsAre("r1", "r2")));
}

Filter TestFilter() { return Filter::Latest(5); }

Matcher<Filter const&> IsTestFilter() {
  return Property(
      &Filter::as_proto,
      Property(&v2::RowFilter::cells_per_column_limit_filter, Eq(5)));
}

ReadModifyWriteRule TestAppendRule() {
  return ReadModifyWriteRule::AppendValue("cf1", "cq1", "append");
}

ReadModifyWriteRule TestIncrementRule() {
  return ReadModifyWriteRule::IncrementAmount("cf2", "cq2", 42);
}

Matcher<v2::ReadModifyWriteRule const> MatchRule(
    ReadModifyWriteRule const& rule) {
  auto const& r = rule.as_proto();
  return AllOf(
      Property(&v2::ReadModifyWriteRule::family_name, r.family_name()),
      Property(&v2::ReadModifyWriteRule::column_qualifier,
               r.column_qualifier()),
      Property(&v2::ReadModifyWriteRule::append_value, r.append_value()),
      Property(&v2::ReadModifyWriteRule::increment_amount,
               r.increment_amount()));
}

TEST(TableTest, ConnectionConstructor) {
  auto conn = std::make_shared<MockDataConnection>();
  auto table = bigtable_internal::MakeTable(conn, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  EXPECT_EQ(kAppProfileId, table.app_profile_id());
  EXPECT_EQ(kProjectId, table.project_id());
  EXPECT_EQ(kInstanceId, table.instance_id());
  EXPECT_EQ(kTableId, table.table_id());
  EXPECT_EQ(kTableName, table.table_name());
}

TEST(TableTest, Apply) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, Apply)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name,
                   bigtable::SingleRowMutation const& mut) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.row_key(), "row");
        return PermanentError();
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto status = table.Apply(IdempotentMutation());
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncApply) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncApply)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name,
                   bigtable::SingleRowMutation const& mut) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.row_key(), "row");
        return make_ready_future(PermanentError());
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto status = table.AsyncApply(IdempotentMutation()).get();
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, BulkApply) {
  std::vector<FailedMutation> expected = {{PermanentError(), 1}};

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, BulkApply)
      .WillOnce([&expected](std::string const& app_profile_id,
                            std::string const& table_name,
                            bigtable::BulkMutation const& mut) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.size(), 2);
        return expected;
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto actual = table.BulkApply(
      BulkMutation(IdempotentMutation(), NonIdempotentMutation()));
  CheckFailedMutations(actual, expected);
}

TEST(TableTest, ReadRows) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name,
                   bigtable::RowSet const& row_set, std::int64_t rows_limit,
                   bigtable::Filter const& filter) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_THAT(row_set, IsTestRowSet());
        EXPECT_EQ(rows_limit, RowReader::NO_ROWS_LIMIT);
        EXPECT_THAT(filter, IsTestFilter());
        return bigtable_mocks::internal::MakeTestRowReader({},
                                                           PermanentError());
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto reader = table.ReadRows(TestRowSet(), TestFilter());
  auto it = reader.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_EQ(++it, reader.end());
}

TEST(TableTest, ReadRowsWithRowLimit) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name,
                   bigtable::RowSet const& row_set, std::int64_t rows_limit,
                   bigtable::Filter const& filter) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_THAT(row_set, IsTestRowSet());
        EXPECT_EQ(rows_limit, 42);
        EXPECT_THAT(filter, IsTestFilter());
        return bigtable_mocks::internal::MakeTestRowReader({},
                                                           PermanentError());
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto reader = table.ReadRows(TestRowSet(), 42, TestFilter());
  auto it = reader.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_EQ(++it, reader.end());
}

TEST(TableTest, ReadRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRow)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name, std::string const& row_key,
                   bigtable::Filter const& filter) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ("row", row_key);
        EXPECT_THAT(filter, IsTestFilter());
        return PermanentError();
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto resp = table.ReadRow("row", TestFilter());
  EXPECT_THAT(resp, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, CheckAndMutateRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name, std::string const& row_key,
                   Filter const& filter,
                   std::vector<Mutation> const& true_mutations,
                   std::vector<Mutation> const& false_mutations) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ("row", row_key);
        EXPECT_THAT(filter, IsTestFilter());
        // We could check individual elements, but verifying the size is good
        // enough for me.
        EXPECT_EQ(1, true_mutations.size());
        EXPECT_EQ(2, false_mutations.size());
        return PermanentError();
      });

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");
  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto row = table.CheckAndMutateRow("row", TestFilter(), {t1}, {f1, f2});
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncCheckAndMutateRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([](std::string const& app_profile_id,
                   std::string const& table_name, std::string const& row_key,
                   Filter const& filter,
                   std::vector<Mutation> const& true_mutations,
                   std::vector<Mutation> const& false_mutations) {
        EXPECT_EQ(kAppProfileId, app_profile_id);
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ("row", row_key);
        EXPECT_THAT(filter, IsTestFilter());
        // We could check individual elements, but verifying the size is good
        // enough for me.
        EXPECT_EQ(1, true_mutations.size());
        EXPECT_EQ(2, false_mutations.size());
        return make_ready_future<StatusOr<bigtable::MutationBranch>>(
            PermanentError());
      });

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");
  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto row =
      table.AsyncCheckAndMutateRow("row", TestFilter(), {t1}, {f1, f2}).get();
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncSampleRows) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncSampleRows)
      .WillOnce(
          [](std::string const& app_profile_id, std::string const& table_name) {
            EXPECT_EQ(kAppProfileId, app_profile_id);
            EXPECT_EQ(kTableName, table_name);
            return make_ready_future<StatusOr<std::vector<RowKeySample>>>(
                PermanentError());
          });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto samples = table.AsyncSampleRows().get();
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, ReadModifyWriteRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfileId, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.rules(),
                    ElementsAre(MatchRule(TestAppendRule()),
                                MatchRule(TestIncrementRule())));
        return PermanentError();
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto row =
      table.ReadModifyWriteRow("row", TestAppendRule(), TestIncrementRule());
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncReadModifyWriteRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([](v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfileId, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.rules(),
                    ElementsAre(MatchRule(TestAppendRule()),
                                MatchRule(TestIncrementRule())));
        return make_ready_future<StatusOr<Row>>(PermanentError());
      });

  auto table = bigtable_internal::MakeTable(mock, kProjectId, kInstanceId,
                                            kAppProfileId, kTableId);
  auto row = table.AsyncReadModifyWriteRow("row", TestAppendRule(),
                                           TestIncrementRule());
  EXPECT_THAT(row.get(), StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
