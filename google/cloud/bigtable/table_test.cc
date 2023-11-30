// Copyright 2017 Google LLC
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
using ::google::cloud::bigtable_mocks::MockDataConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Return;
using ::testing::Unused;

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

// Set by connection, client, and operation.
struct TestOption1 {
  using Type = std::string;
};

// Set by connection and client.
struct TestOption2 {
  using Type = std::string;
};

// Set by connection.
struct TestOption3 {
  using Type = std::string;
};

Options CallOptions() { return Options{}.set<TestOption1>("call"); }

Options TableOptions() {
  return Options{}
      .set<AppProfileIdOption>(kAppProfileId)
      .set<TestOption1>("client")
      .set<TestOption2>("client");
}

Options ConnectionOptions() {
  return Options{}
      .set<TestOption1>("connection")
      .set<TestOption2>("connection")
      .set<TestOption3>("connection");
}

Table TestTable(std::shared_ptr<MockDataConnection> mock) {
  EXPECT_CALL(*mock, options).WillRepeatedly(Return(ConnectionOptions()));
  return Table(std::move(mock),
               TableResource(kProjectId, kInstanceId, kTableId),
               TableOptions());
}

void CheckCurrentOptions() {
  auto const& options = google::cloud::internal::CurrentOptions();
  EXPECT_EQ(kAppProfileId, options.get<AppProfileIdOption>());
  EXPECT_EQ("call", options.get<TestOption1>());
  EXPECT_EQ("client", options.get<TestOption2>());
  EXPECT_EQ("connection", options.get<TestOption3>());
}

TEST(TableTest, ConnectionConstructor) {
  auto conn = std::make_shared<MockDataConnection>();
  auto table =
      Table(std::move(conn), TableResource(kProjectId, kInstanceId, kTableId));
  EXPECT_EQ(kProjectId, table.project_id());
  EXPECT_EQ(kInstanceId, table.instance_id());
  EXPECT_EQ(kTableId, table.table_id());
  EXPECT_EQ(kTableName, table.table_name());
}

TEST(TableTest, AppProfileId) {
  auto conn = std::make_shared<MockDataConnection>();
  auto table = Table(conn, TableResource(kProjectId, kInstanceId, kTableId));
  EXPECT_EQ("", table.app_profile_id());

  table = Table(conn, TableResource(kProjectId, kInstanceId, kTableId),
                Options{}.set<AppProfileIdOption>(kAppProfileId));
  EXPECT_EQ(kAppProfileId, table.app_profile_id());
}

TEST(TableTest, Apply) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, Apply)
      .WillOnce([](std::string const& table_name,
                   bigtable::SingleRowMutation const& mut) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.row_key(), "row");
        return PermanentError();
      });

  auto table = TestTable(std::move(mock));
  auto status = table.Apply(IdempotentMutation(), CallOptions());
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncApply) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncApply)
      .WillOnce([](std::string const& table_name,
                   bigtable::SingleRowMutation const& mut) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.row_key(), "row");
        return make_ready_future(PermanentError());
      });

  auto table = TestTable(std::move(mock));
  auto status = table.AsyncApply(IdempotentMutation(), CallOptions()).get();
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, BulkApply) {
  std::vector<FailedMutation> expected = {{PermanentError(), 1}};

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, BulkApply)
      .WillOnce([&expected](std::string const& table_name,
                            bigtable::BulkMutation const& mut) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.size(), 2);
        return expected;
      });

  auto table = TestTable(std::move(mock));
  auto actual = table.BulkApply(
      BulkMutation(IdempotentMutation(), NonIdempotentMutation()),
      CallOptions());
  CheckFailedMutations(actual, expected);
}

TEST(TableTest, AsyncBulkApply) {
  std::vector<FailedMutation> expected = {{PermanentError(), 1}};

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncBulkApply)
      .WillOnce([&expected](std::string const& table_name,
                            bigtable::BulkMutation const& mut) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ(mut.size(), 2);
        return make_ready_future(expected);
      });

  auto table = TestTable(std::move(mock));
  auto actual = table.AsyncBulkApply(
      BulkMutation(IdempotentMutation(), NonIdempotentMutation()),
      CallOptions());
  CheckFailedMutations(actual.get(), expected);
}

TEST(TableTest, ReadRows) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRowsFull).WillOnce([](ReadRowsParams const& params) {
    CheckCurrentOptions();
    EXPECT_EQ(params.table_name, kTableName);
    EXPECT_EQ(params.app_profile_id, kAppProfileId);
    EXPECT_THAT(params.row_set, IsTestRowSet());
    EXPECT_EQ(params.rows_limit, RowReader::NO_ROWS_LIMIT);
    EXPECT_THAT(params.filter, IsTestFilter());
    return bigtable_mocks::MakeRowReader({}, PermanentError());
  });

  auto table = TestTable(std::move(mock));
  auto reader = table.ReadRows(TestRowSet(), TestFilter(), CallOptions());
  auto it = reader.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_EQ(++it, reader.end());
}

TEST(TableTest, ReadRowsWithRowLimit) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRowsFull).WillOnce([](ReadRowsParams const& params) {
    CheckCurrentOptions();
    EXPECT_EQ(params.table_name, kTableName);
    EXPECT_EQ(params.app_profile_id, kAppProfileId);
    EXPECT_THAT(params.row_set, IsTestRowSet());
    EXPECT_EQ(params.rows_limit, 42);
    EXPECT_THAT(params.filter, IsTestFilter());
    return bigtable_mocks::MakeRowReader({}, PermanentError());
  });

  auto table = TestTable(std::move(mock));
  auto reader = table.ReadRows(TestRowSet(), 42, TestFilter(), CallOptions());
  auto it = reader.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_EQ(++it, reader.end());
}

TEST(TableTest, ReadRowsMockBackwardsCompatibility) {
  auto mock = std::make_shared<MockDataConnection>();
  // Ensure that existing mocks which set expectations on the legacy `ReadRows`
  // method continue to work. This is more a test of `MockDataConnection` than
  // of `Table`.
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::string const& table_name,
                   bigtable::RowSet const& row_set, std::int64_t rows_limit,
                   bigtable::Filter const& filter) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_THAT(row_set, IsTestRowSet());
        EXPECT_EQ(rows_limit, 42);
        EXPECT_THAT(filter, IsTestFilter());
        return bigtable_mocks::MakeRowReader({}, PermanentError());
      });

  auto table = TestTable(std::move(mock));
  auto reader = table.ReadRows(TestRowSet(), 42, TestFilter(), CallOptions());
  auto it = reader.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_EQ(++it, reader.end());
}

TEST(TableTest, ReadRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRow)
      .WillOnce([](std::string const& table_name, std::string const& row_key,
                   bigtable::Filter const& filter) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ("row", row_key);
        EXPECT_THAT(filter, IsTestFilter());
        return PermanentError();
      });

  auto table = TestTable(std::move(mock));
  auto resp = table.ReadRow("row", TestFilter(), CallOptions());
  EXPECT_THAT(resp, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, CheckAndMutateRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([](std::string const& table_name, std::string const& row_key,
                   Filter const& filter,
                   std::vector<Mutation> const& true_mutations,
                   std::vector<Mutation> const& false_mutations) {
        CheckCurrentOptions();
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
  auto table = TestTable(std::move(mock));
  auto row = table.CheckAndMutateRow("row", TestFilter(), {t1}, {f1, f2},
                                     CallOptions());
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncCheckAndMutateRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([](std::string const& table_name, std::string const& row_key,
                   Filter const& filter,
                   std::vector<Mutation> const& true_mutations,
                   std::vector<Mutation> const& false_mutations) {
        CheckCurrentOptions();
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
  auto table = TestTable(std::move(mock));
  auto row = table.AsyncCheckAndMutateRow("row", TestFilter(), {t1}, {f1, f2},
                                          CallOptions());
  EXPECT_THAT(row.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, SampleRows) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, SampleRows).WillOnce([](std::string const& table_name) {
    CheckCurrentOptions();
    EXPECT_EQ(kTableName, table_name);
    return PermanentError();
  });

  auto table = TestTable(std::move(mock));
  auto samples = table.SampleRows(CallOptions());
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncSampleRows) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncSampleRows)
      .WillOnce([](std::string const& table_name) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        return make_ready_future<StatusOr<std::vector<RowKeySample>>>(
            PermanentError());
      });

  auto table = TestTable(std::move(mock));
  auto samples = table.AsyncSampleRows(CallOptions()).get();
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, ReadModifyWriteRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](v2::ReadModifyWriteRowRequest const& request) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.rules(),
                    ElementsAre(MatchRule(TestAppendRule()),
                                MatchRule(TestIncrementRule())));
        return PermanentError();
      });

  auto table = TestTable(std::move(mock));
  auto row = table.ReadModifyWriteRow("row", TestAppendRule(),
                                      TestIncrementRule(), CallOptions());
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, ReadModifyWriteRowOptionsMerge) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](v2::ReadModifyWriteRowRequest const& request) {
        auto const& options = google::cloud::internal::CurrentOptions();
        EXPECT_EQ("latter", options.get<TestOption1>());
        EXPECT_EQ("former", options.get<TestOption2>());
        EXPECT_EQ("latter", options.get<TestOption3>());
        EXPECT_THAT(request.rules(),
                    ElementsAre(MatchRule(TestAppendRule()),
                                MatchRule(TestIncrementRule())));
        return PermanentError();
      });

  auto former = Options{}.set<TestOption1>("former").set<TestOption2>("former");
  auto latter = Options{}.set<TestOption1>("latter").set<TestOption3>("latter");

  auto table = TestTable(std::move(mock));
  (void)table.ReadModifyWriteRow("row", TestAppendRule(), former,
                                 TestIncrementRule(), latter);
}

TEST(TableTest, AsyncReadModifyWriteRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([](v2::ReadModifyWriteRowRequest const& request) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.rules(),
                    ElementsAre(MatchRule(TestAppendRule()),
                                MatchRule(TestIncrementRule())));
        return make_ready_future<StatusOr<Row>>(PermanentError());
      });

  auto table = TestTable(std::move(mock));
  auto row = table.AsyncReadModifyWriteRow("row", TestAppendRule(),
                                           TestIncrementRule(), CallOptions());
  EXPECT_THAT(row.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(TableTest, AsyncReadModifyWriteRowOptionsMerge) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([](v2::ReadModifyWriteRowRequest const& request) {
        auto const& options = google::cloud::internal::CurrentOptions();
        EXPECT_EQ("latter", options.get<TestOption1>());
        EXPECT_EQ("former", options.get<TestOption2>());
        EXPECT_EQ("latter", options.get<TestOption3>());
        EXPECT_THAT(request.rules(),
                    ElementsAre(MatchRule(TestAppendRule()),
                                MatchRule(TestIncrementRule())));
        return make_ready_future<StatusOr<Row>>(PermanentError());
      });

  auto former = Options{}.set<TestOption1>("former").set<TestOption2>("former");
  auto latter = Options{}.set<TestOption1>("latter").set<TestOption3>("latter");

  auto table = TestTable(std::move(mock));
  (void)table
      .AsyncReadModifyWriteRow("row", TestAppendRule(), former,
                               TestIncrementRule(), latter)
      .get();
}

TEST(TableTest, AsyncReadRows) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](std::string const& table_name,
                   std::function<future<bool>(bigtable::Row)> const& on_row,
                   std::function<void(Status)> const& on_finish,
                   bigtable::RowSet const& row_set, std::int64_t rows_limit,
                   bigtable::Filter const& filter) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_THAT(row_set, IsTestRowSet());
        EXPECT_EQ(RowReader::NO_ROWS_LIMIT, rows_limit);
        EXPECT_THAT(filter, IsTestFilter());

        // Invoke the callbacks.
        EXPECT_TRUE(on_row(bigtable::Row("r1", {})).get());
        EXPECT_FALSE(on_row(bigtable::Row("r2", {})).get());
        on_finish(PermanentError());
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r2", row.row_key());
        return make_ready_future(false);
      });

  MockFunction<void(Status)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  });

  auto table = TestTable(std::move(mock));
  table.AsyncReadRows(on_row.AsStdFunction(), on_finish.AsStdFunction(),
                      TestRowSet(), TestFilter(), CallOptions());
}

TEST(TableTest, AsyncReadRowsWithRowLimit) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](std::string const& table_name,
                   std::function<future<bool>(bigtable::Row)> const& on_row,
                   std::function<void(Status)> const& on_finish,
                   bigtable::RowSet const& row_set, std::int64_t rows_limit,
                   bigtable::Filter const& filter) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_THAT(row_set, IsTestRowSet());
        EXPECT_EQ(42, rows_limit);
        EXPECT_THAT(filter, IsTestFilter());

        // Invoke the callbacks.
        EXPECT_TRUE(on_row(bigtable::Row("r1", {})).get());
        EXPECT_FALSE(on_row(bigtable::Row("r2", {})).get());
        on_finish(PermanentError());
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r2", row.row_key());
        return make_ready_future(false);
      });

  MockFunction<void(Status)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  });

  auto table = TestTable(std::move(mock));
  table.AsyncReadRows(on_row.AsStdFunction(), on_finish.AsStdFunction(),
                      TestRowSet(), 42, TestFilter(), CallOptions());
}

TEST(TableTest, AsyncReadRowsAcceptsMoveOnlyTypes) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused,
                   std::function<future<bool>(bigtable::Row)> const& on_row,
                   std::function<void(Status)> const& on_finish, Unused, Unused,
                   Unused) {
        // Invoke the callbacks.
        EXPECT_TRUE(on_row(bigtable::Row("row", {})).get());
        on_finish(PermanentError());
      });

  class MoveOnly {
   public:
    MoveOnly() = default;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;

    future<bool> operator()(Row const& row) {
      EXPECT_EQ("row", row.row_key());
      return make_ready_future(true);
    }

    void operator()(Status const& status) {
      EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
    }
  };

  auto table = TestTable(std::move(mock));
  table.AsyncReadRows(MoveOnly{}, MoveOnly{}, TestRowSet(), TestFilter());
}

TEST(TableTest, AsyncReadRow) {
  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRow)
      .WillOnce([](std::string const& table_name, std::string const& row_key,
                   bigtable::Filter const& filter) {
        CheckCurrentOptions();
        EXPECT_EQ(kTableName, table_name);
        EXPECT_EQ("row", row_key);
        EXPECT_THAT(filter, IsTestFilter());
        return make_ready_future<StatusOr<std::pair<bool, bigtable::Row>>>(
            PermanentError());
      });

  auto table = TestTable(std::move(mock));
  auto resp = table.AsyncReadRow("row", TestFilter(), CallOptions()).get();
  EXPECT_THAT(resp, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
