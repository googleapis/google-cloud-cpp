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

#include "bigtable/client/testing/chrono_literals.h"
#include "bigtable/client/testing/internal_table_test_fixture.h"

using testing::_;
using testing::Return;
using testing::SetArgPointee;
using namespace bigtable::chrono_literals;

/// Define types and functions used in the tests.
namespace {
class TableTest : public bigtable::testing::internal::TableTestFixture {};
class TableReadRowTest : public bigtable::testing::internal::TableTestFixture {
};
class TableReadRowsTest : public bigtable::testing::internal::TableTestFixture {
};
class TableApplyTest : public bigtable::testing::internal::TableTestFixture {};

class TableSampleRowKeysTest
    : public bigtable::testing::internal::TableTestFixture {};
class MockReader : public grpc::ClientReaderInterface<
                       ::google::bigtable::v2::MutateRowsResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1(Read, bool(::google::bigtable::v2::MutateRowsResponse*));
};

class MockReaderSampleRowKeysResponse
    : public grpc::ClientReaderInterface<
          ::google::bigtable::v2::SampleRowKeysResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1(Read, bool(::google::bigtable::v2::SampleRowKeysResponse*));
};

class TableBulkApplyTest
    : public bigtable::testing::internal::TableTestFixture {};

class TableCheckAndMutateRowTest
    : public bigtable::testing::internal::TableTestFixture {};
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

TEST_F(TableReadRowTest, ReadRowSimple) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  grpc::Status status;
  auto response =
      bigtable::testing::internal::ReadRowsResponseFromString(R"(
                                                                  chunks {
                                                                  row_key: "r1"
                                                                      family_name { value: "fam" }
                                                                      qualifier { value: "col" }
                                                                  timestamp_micros: 42000
                                                                  value: "value"
                                                                  commit_row: true
                                                                  }
                                                                  )",
                                                              status);
  EXPECT_TRUE(status.ok());

  auto stream =
      bigtable::internal::make_unique<bigtable::testing::MockResponseStream>();
  EXPECT_CALL(*stream, Read(_))
      .WillOnce(Invoke([&response](btproto::ReadRowsResponse* r) {
        *r = response;
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
      .WillOnce(Invoke([&stream, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
        EXPECT_EQ(1, req.rows().row_keys_size());
        EXPECT_EQ("r1", req.rows().row_keys(0));
        EXPECT_EQ(1, req.rows_limit());
        EXPECT_EQ(table_.table_name(), req.table_name());
        return stream.release();
      }));

  auto result = table_.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(std::get<0>(result));
  auto row = std::get<1>(result);
  EXPECT_EQ("r1", row.row_key());
}

TEST_F(TableReadRowTest, ReadRowMissing) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto stream =
      bigtable::internal::make_unique<bigtable::testing::MockResponseStream>();
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
      .WillOnce(Invoke([&stream, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
        EXPECT_EQ(1, req.rows().row_keys_size());
        EXPECT_EQ("r1", req.rows().row_keys(0));
        EXPECT_EQ(1, req.rows_limit());
        EXPECT_EQ(table_.table_name(), req.table_name());
        return stream.release();
      }));
  grpc::Status status;
  auto result = table_.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(std::get<0>(result));
}

TEST_F(TableReadRowTest, ReadRowError) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto stream =
      bigtable::internal::make_unique<bigtable::testing::MockResponseStream>();
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::INTERNAL, "Internal Error")));

  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
      .WillOnce(Invoke([&stream, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
        EXPECT_EQ(1, req.rows().row_keys_size());
        EXPECT_EQ("r1", req.rows().row_keys(0));
        EXPECT_EQ(1, req.rows_limit());
        EXPECT_EQ(table_.table_name(), req.table_name());
        return stream.release();
      }));
  grpc::Status status;
  auto result = table_.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(std::get<0>(result));
}

TEST_F(TableReadRowsTest, ReadRowsCanReadOneRow) {
  grpc::Status status;
  auto response =
      bigtable::testing::internal::ReadRowsResponseFromString(R"(
                                                                  chunks {
                                                                  row_key: "r1"
                                                                      family_name { value: "fam" }
                                                                      qualifier { value: "qual" }
                                                                  timestamp_micros: 42000
                                                                  value: "value"
                                                                  commit_row: true
                                                                  }
                                                                  )",
                                                              status);
  EXPECT_TRUE(status.ok());

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  auto stream = new bigtable::testing::MockResponseStream;
  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream));
  EXPECT_CALL(*stream, Read(testing::_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
  status = reader.Finish();
  EXPECT_TRUE(status.ok());
}

TEST_F(TableReadRowsTest, ReadRowsCanReadWithRetries) {
  grpc::Status status;
  auto response =
      bigtable::testing::internal::ReadRowsResponseFromString(R"(
                                                                  chunks {
                                                                  row_key: "r1"
                                                                      family_name { value: "fam" }
                                                                      qualifier { value: "qual" }
                                                                  timestamp_micros: 42000
                                                                  value: "value"
                                                                  commit_row: true
                                                                  }
                                                                  )",
                                                              status);
  EXPECT_TRUE(status.ok());

  auto response_retry =
      bigtable::testing::internal::ReadRowsResponseFromString(R"(
                                                                        chunks {
                                                                        row_key: "r2"
                                                                            family_name { value: "fam" }
                                                                            qualifier { value: "qual" }
                                                                        timestamp_micros: 42000
                                                                        value: "value"
                                                                        commit_row: true
                                                                        }
                                                                        )",
                                                              status);
  EXPECT_TRUE(status.ok());

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  auto stream = new bigtable::testing::MockResponseStream;
  auto stream_retry = new bigtable::testing::MockResponseStream;

  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
      .WillOnce(Return(stream))
      .WillOnce(Return(stream_retry));

  EXPECT_CALL(*stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*stream, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_CALL(*stream_retry, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response_retry), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  ++it;
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
  status = reader.Finish();
  EXPECT_TRUE(status.ok());
}

TEST_F(TableReadRowsTest, ReadRowsThrowsWhenTooManyErrors) {
  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
      .WillRepeatedly(testing::WithoutArgs(testing::Invoke([] {
        auto stream = new bigtable::testing::MockResponseStream;
        EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
        EXPECT_CALL(*stream, Finish())
            .WillOnce(
                Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "broken")));
        return stream;
      })));

  auto table = bigtable::noex::Table(
      client_, "table_id", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(std::chrono::seconds(0),
                                         std::chrono::seconds(0)),
      bigtable::SafeIdempotentMutationPolicy());
  auto reader =
      table.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  reader.begin();
  grpc::Status status = reader.Finish();
  EXPECT_FALSE(status.ok());
}

/// @test Verify that Table::Apply() works in a simplest case.
TEST_F(TableApplyTest, Simple) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillOnce(Return(grpc::Status::OK));

  auto result = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_TRUE(result.empty());
}

/// @test Verify that Table::Apply() raises an exception on permanent failures.
TEST_F(TableApplyTest, Failure) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh")));
  std::vector<bigtable::FailedMutation> result;
  result = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(1UL, result.size());
  EXPECT_FALSE(result.front().status().ok());
}

/// @test Verify that Table::Apply() retries on partial failures.
TEST_F(TableApplyTest, Retry) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Return(grpc::Status::OK));
  auto result = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_TRUE(result.empty());
}

/// @test Verify that Table::Apply() retries only idempotent mutations.
TEST_F(TableApplyTest, RetryIdempotent) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
  auto result = table_.Apply(bigtable::SingleRowMutation(
      "not-idempotent", {bigtable::SetCell("fam", "col", "val")}));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(1UL, result.size());
  EXPECT_FALSE(result.front().status().ok());
}

/// @test Verify that Table::BulkApply() works in the easy case.
TEST_F(TableBulkApplyTest, Simple) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto reader = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&reader](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return reader.release();
          }));
  grpc::Status status;
  table_.BulkApply(
      bt::BulkMutation(bt::SingleRowMutation(
                           "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                       bt::SingleRowMutation(
                           "bar", {bt::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_TRUE(status.ok());
  SUCCEED();
}

/// @test Verify that Table::BulkApply() retries partial failures.
TEST_F(TableBulkApplyTest, RetryPartialFailure) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial (recoverable) failure.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r2.release();
          }));
  grpc::Status status;
  table_.BulkApply(
      bt::BulkMutation(
          bt::SingleRowMutation("foo",
                                {bigtable::SetCell("fam", "col", 0_ms, "baz")}),
          bt::SingleRowMutation(
              "bar", {bigtable::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_TRUE(status.ok());
  SUCCEED();
}

// TODO(#234) - this test could be enabled when bug is closed.
/// @test Verify that Table::BulkApply() handles permanent failures.
TEST_F(TableBulkApplyTest, PermanentFailure) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::OUT_OF_RANGE);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }));
  grpc::Status status;
  table_.BulkApply(
      bt::BulkMutation(bt::SingleRowMutation(
                           "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                       bt::SingleRowMutation(
                           "bar", {bt::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that Table::BulkApply() handles a terminated stream.
TEST_F(TableBulkApplyTest, CanceledStream) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  // Simulate a stream that returns one success and then terminates.  We expect
  // the BulkApply() operation to retry the request, because the mutation is in
  // an undetermined state.  Well, it should retry assuming it is idempotent,
  // which happens to be the case in this test.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // Create a second stream returned by the mocks when the client retries.
  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r2.release();
          }));
  grpc::Status status;
  table_.BulkApply(
      bt::BulkMutation(bt::SingleRowMutation(
                           "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                       bt::SingleRowMutation(
                           "bar", {bt::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_TRUE(status.ok());
  SUCCEED();
}

/// @test Verify that Table::BulkApply() reports correctly on too many errors.
TEST_F(TableBulkApplyTest, TooManyFailures) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  namespace btnoex = ::bigtable::noex;

  using namespace bigtable::chrono_literals;
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  btnoex::Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      bt::LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      bt::ExponentialBackoffPolicy(10_us, 40_us),
      // TODO(#66) - it is annoying to set a policy we do not care about.
      bt::SafeIdempotentMutationPolicy());

  // Setup the mocks to fail more than 3 times.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));

  auto create_cancelled_stream = [&](grpc::ClientContext*,
                                     btproto::MutateRowsRequest const&) {
    auto stream = bigtable::internal::make_unique<MockReader>();
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream.release();
  };

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }))
      .WillOnce(Invoke(create_cancelled_stream))
      .WillOnce(Invoke(create_cancelled_stream));
  grpc::Status status;
  custom_table.BulkApply(
      bt::BulkMutation(bt::SingleRowMutation(
                           "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                       bt::SingleRowMutation(
                           "bar", {bt::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that Table::BulkApply() retries only idempotent mutations.
TEST_F(TableBulkApplyTest, RetryOnlyIdempotent) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  // We will send both idempotent and non-idempotent mutations.  We prepare the
  // mocks to return an empty stream in the first RPC request.  That will force
  // the client to only retry the idempotent mutations.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r2.release();
          }));
  grpc::Status status;
  auto result = table_.BulkApply(
      bt::BulkMutation(
          bt::SingleRowMutation("is-idempotent",
                                {bt::SetCell("fam", "col", 0_ms, "qux")}),
          bt::SingleRowMutation("not-idempotent",
                                {bt::SetCell("fam", "col", "baz")})),
      status);
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(1UL, result.size());
  EXPECT_EQ(1, result[0].original_index());
  EXPECT_EQ("not-idempotent", result[0].mutation().row_key());
}

/// @test Verify that Table::BulkApply() works when the RPC fails.
TEST_F(TableBulkApplyTest, FailedRPC) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto reader = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*reader, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                    "no such table")));

  EXPECT_CALL(*bigtable_stub_, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&reader](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return reader.release();
          }));
  std::vector<bigtable::FailedMutation> result;
  grpc::Status status;
  result = table_.BulkApply(
      bt::BulkMutation(bt::SingleRowMutation(
                           "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                       bt::SingleRowMutation(
                           "bar", {bt::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
  EXPECT_EQ("no such table", status.error_message());
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(2UL, result.size());
}

/// @test Verify that Table::CheckAndMutateRow() works in a simplest case.
TEST_F(TableCheckAndMutateRowTest, Simple) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, CheckAndMutateRow(_, _, _))
      .WillOnce(Return(grpc::Status::OK));
  grpc::Status status;
  table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}, status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify that Table::CheckAndMutateRow() raises an on failures.
TEST_F(TableCheckAndMutateRowTest, Failure) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, CheckAndMutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
  grpc::Status status;
  table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}, status);
  EXPECT_FALSE(status.ok());
}

/// @test Verify that Table::SampleRows<T>() works for default parameter.
TEST_F(TableSampleRowKeysTest, DefaultParameterTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReaderSampleRowKeysResponse;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _)).WillOnce(Return(reader));
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));
  grpc::Status status;
  std::vector<bigtable::RowKeySample> result = table_.SampleRows<>(status);
  EXPECT_TRUE(status.ok());
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

/// @test Verify that Table::SampleRows<T>() works for std::vector.
TEST_F(TableSampleRowKeysTest, SimpleVectorTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReaderSampleRowKeysResponse;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _)).WillOnce(Return(reader));
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));
  grpc::Status status;
  std::vector<bigtable::RowKeySample> result =
      table_.SampleRows<std::vector>(status);
  EXPECT_TRUE(status.ok());
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

/// @test Verify that Table::SampleRows<T>() works for std::list.
TEST_F(TableSampleRowKeysTest, SimpleListTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReaderSampleRowKeysResponse;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _)).WillOnce(Return(reader));
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));
  grpc::Status status;
  std::list<bigtable::RowKeySample> result =
      table_.SampleRows<std::list>(status);
  EXPECT_TRUE(status.ok());
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

TEST_F(TableSampleRowKeysTest, SampleRowKeysRetryTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = new MockReaderSampleRowKeysResponse;
  auto reader_retry = new MockReaderSampleRowKeysResponse;
  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _))
      .WillOnce(Return(reader))
      .WillOnce(Return(reader_retry));

  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));

  EXPECT_CALL(*reader, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_CALL(*reader_retry, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test2");
          r->set_offset_bytes(123);
        }
        return true;
      }))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test3");
          r->set_offset_bytes(1234);
        }
        return true;
      }))
      .WillOnce(Return(false));

  EXPECT_CALL(*reader_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  grpc::Status status;
  auto results = table_.SampleRows<std::vector>(status);
  EXPECT_TRUE(status.ok());

  auto it = results.begin();
  EXPECT_NE(it, results.end());
  EXPECT_EQ("test2", it->row_key);
  ++it;
  EXPECT_NE(it, results.end());
  EXPECT_EQ("test3", it->row_key);
  EXPECT_EQ(++it, results.end());
}

/// @test Verify that Table::sample_rows() reports correctly on too many errors.
TEST_F(TableSampleRowKeysTest, TooManyFailures) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  using namespace bigtable::chrono_literals;
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  ::bigtable::noex::Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      ::bigtable::LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      ::bigtable::ExponentialBackoffPolicy(10_us, 40_us),
      ::bigtable::SafeIdempotentMutationPolicy());

  // Setup the mocks to fail more than 3 times.
  auto r1 = new MockReaderSampleRowKeysResponse;
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::SampleRowKeysResponse* r) {
        {
          r->set_row_key("test1");
          r->set_offset_bytes(11);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));

  auto create_cancelled_stream = [&](grpc::ClientContext*,
                                     btproto::SampleRowKeysRequest const&) {
    auto stream = new MockReaderSampleRowKeysResponse;
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream;
  };

  EXPECT_CALL(*bigtable_stub_, SampleRowKeysRaw(_, _))
      .WillOnce(Return(r1))
      .WillOnce(Invoke(create_cancelled_stream))
      .WillOnce(Invoke(create_cancelled_stream));
  grpc::Status status;
  custom_table.SampleRows<std::vector>(status);
  EXPECT_FALSE(status.ok());
}
