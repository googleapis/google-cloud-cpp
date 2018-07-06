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

#include "google/cloud/bigtable/testing/chrono_literals.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_sample_row_keys_reader.h"

namespace bigtable = google::cloud::bigtable;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using namespace bigtable::chrono_literals;
namespace btproto = google::bigtable::v2;

/// Define types and functions used in the tests.
namespace {
class NoexTableTest : public bigtable::testing::internal::TableTestFixture {};

using bigtable::testing::MockMutateRowsReader;
using bigtable::testing::MockReadRowsReader;
using bigtable::testing::MockSampleRowKeysReader;

/**
 * Helper class to create the expectations for a simple RPC call.
 *
 * Given the type of the request and responses, this struct provides a function
 * to create a mock implementation with the right signature and checks.
 *
 * @tparam RequestType the protobuf type for the request.
 * @tparam ResponseType the protobuf type for the response.
 */
template <typename RequestType, typename ResponseType>
struct MockRpcFactory {
  using SignatureType = grpc::Status(grpc::ClientContext* ctx,
                                     RequestType const& request,
                                     ResponseType* response);

  /// Refactor the boilerplate common to most tests.
  static std::function<SignatureType> Create(std::string expected_id) {
    return std::function<SignatureType>(
        [expected_id](grpc::ClientContext* ctx, RequestType const& request,
                      ResponseType* response) {
          RequestType expected;
          // Cannot use ASSERT_TRUE() here, it has an embedded "return;"
          std::string delta;
          EXPECT_EQ(expected_id, request.app_profile_id());

          EXPECT_NE(nullptr, response);
          return grpc::Status::OK;
        });
  }
};

}  // anonymous namespace

TEST_F(NoexTableTest, ChangeOnePolicy) {
  bigtable::noex::Table table(client_, "some-table",
                              bigtable::AlwaysRetryMutationPolicy());
  EXPECT_THAT(table.table_name(), ::testing::HasSubstr("some-table"));
}

TEST_F(NoexTableTest, ChangePolicies) {
  bigtable::noex::Table table(client_, "some-table",
                              bigtable::AlwaysRetryMutationPolicy(),
                              bigtable::LimitedErrorCountRetryPolicy(42));
  EXPECT_THAT(table.table_name(), ::testing::HasSubstr("some-table"));
}

TEST_F(NoexTableTest, ClientProjectId) {
  EXPECT_EQ(kProjectId, client_->project_id());
}

TEST_F(NoexTableTest, ClientInstanceId) {
  EXPECT_EQ(kInstanceId, client_->instance_id());
}

TEST_F(NoexTableTest, StandaloneInstanceName) {
  EXPECT_EQ(kInstanceName, bigtable::InstanceName(client_));
}

TEST_F(NoexTableTest, StandaloneTableName) {
  EXPECT_EQ(kTableName, bigtable::TableName(client_, kTableId));
}

TEST_F(NoexTableTest, TableName) {
  // clang-format: you are drunk, placing this short function in a single line
  // is not nice.
  EXPECT_EQ(kTableName, table_.table_name());
}

TEST_F(NoexTableTest, TableConstructor) {
  std::string const kOtherTableId = "my-table";
  std::string const kOtherTableName =
      bigtable::TableName(client_, kOtherTableId);
  bigtable::Table table(client_, kOtherTableId);
  EXPECT_EQ(kOtherTableName, table.table_name());
}

TEST_F(NoexTableTest, CopyConstructor) {
  bigtable::noex::Table source(client_, "my-table");
  std::string expected = source.table_name();
  bigtable::noex::Table copy(source);
  EXPECT_EQ(expected, copy.table_name());
}

TEST_F(NoexTableTest, MoveConstructor) {
  bigtable::noex::Table source(client_, "my-table");
  std::string expected = source.table_name();
  bigtable::noex::Table copy(std::move(source));
  EXPECT_EQ(expected, copy.table_name());
}

TEST_F(NoexTableTest, CopyAssignment) {
  bigtable::noex::Table source(client_, "my-table");
  std::string expected = source.table_name();
  bigtable::noex::Table dest(client_, "another-table");
  dest = source;
  EXPECT_EQ(expected, dest.table_name());
}

TEST_F(NoexTableTest, MoveAssignment) {
  bigtable::noex::Table source(client_, "my-table");
  std::string expected = source.table_name();
  bigtable::noex::Table dest(client_, "another-table");
  dest = std::move(source);
  EXPECT_EQ(expected, dest.table_name());
}

TEST_F(NoexTableTest, ReadRowSimple) {
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

  auto stream = bigtable::internal::make_unique<MockReadRowsReader>();
  EXPECT_CALL(*stream, Read(_))
      .WillOnce(Invoke([&response](btproto::ReadRowsResponse* r) {
        *r = response;
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke([&stream, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
        EXPECT_EQ(1, req.rows().row_keys_size());
        EXPECT_EQ("r1", req.rows().row_keys(0));
        EXPECT_EQ(1, req.rows_limit());
        EXPECT_EQ(table_.table_name(), req.table_name());
        return stream.release()->AsUniqueMocked();
      }));

  auto result = table_.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(std::get<0>(result));
  auto row = std::get<1>(result);
  EXPECT_EQ("r1", row.row_key());
}

TEST_F(NoexTableTest, ReadRow_AppProfileId) {
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

  auto stream = bigtable::internal::make_unique<MockReadRowsReader>();
  EXPECT_CALL(*stream, Read(_))
      .WillOnce(Invoke([&response](btproto::ReadRowsResponse* r) {
        *r = response;
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  std::string expected_id = "test-id";
  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke(
          [&stream, expected_id, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
            EXPECT_EQ(1, req.rows().row_keys_size());
            EXPECT_EQ("r1", req.rows().row_keys(0));
            EXPECT_EQ(1, req.rows_limit());
            EXPECT_EQ(table_.table_name(), req.table_name());
            EXPECT_EQ(expected_id, req.app_profile_id());
            return stream.release()->AsUniqueMocked();
          }));

  bigtable::AppProfileId app_profile_id("test-id");
  bigtable::noex::Table table =
      bigtable::noex::Table(client_, app_profile_id, kTableId);
  auto result = table.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(std::get<0>(result));
  auto row = std::get<1>(result);
  EXPECT_EQ("r1", row.row_key());
}

TEST_F(NoexTableTest, ReadRowMissing) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto stream = bigtable::internal::make_unique<MockReadRowsReader>();
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke([&stream, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
        EXPECT_EQ(1, req.rows().row_keys_size());
        EXPECT_EQ("r1", req.rows().row_keys(0));
        EXPECT_EQ(1, req.rows_limit());
        EXPECT_EQ(table_.table_name(), req.table_name());
        return stream.release()->AsUniqueMocked();
      }));
  grpc::Status status;
  auto result = table_.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(std::get<0>(result));
}

TEST_F(NoexTableTest, ReadRowError) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto stream = bigtable::internal::make_unique<MockReadRowsReader>();
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::INTERNAL, "Internal Error")));

  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke([&stream, this](grpc::ClientContext*,
                                       btproto::ReadRowsRequest const& req) {
        EXPECT_EQ(1, req.rows().row_keys_size());
        EXPECT_EQ("r1", req.rows().row_keys(0));
        EXPECT_EQ(1, req.rows_limit());
        EXPECT_EQ(table_.table_name(), req.table_name());
        return stream.release()->AsUniqueMocked();
      }));
  grpc::Status status;
  auto result = table_.ReadRow("r1", bigtable::Filter::PassAllFilter(), status);
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(std::get<0>(result));
}

TEST_F(NoexTableTest, ReadRowsCanReadOneRow) {
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

  auto stream = bigtable::internal::make_unique<MockReadRowsReader>();
  EXPECT_CALL(*stream, Read(testing::_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke(stream.release()->MakeMockReturner()));

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
  status = reader.Finish();
  EXPECT_TRUE(status.ok());
}

TEST_F(NoexTableTest, ReadRowsCanReadWithRetries) {
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

  auto stream = bigtable::internal::make_unique<MockReadRowsReader>();
  auto stream_retry = bigtable::internal::make_unique<MockReadRowsReader>();

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

  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke(stream.release()->MakeMockReturner()))
      .WillOnce(Invoke(stream_retry.release()->MakeMockReturner()));

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

TEST_F(NoexTableTest, ReadRowsThrowsWhenTooManyErrors) {
  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillRepeatedly(testing::Invoke([](grpc::ClientContext*,
                                         btproto::ReadRowsRequest const&) {
        auto stream = new MockReadRowsReader;
        EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
        EXPECT_CALL(*stream, Finish())
            .WillOnce(
                Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "broken")));
        return stream->AsUniqueMocked();
      }));

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
TEST_F(NoexTableTest, ApplySimple) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _)).WillOnce(Return(grpc::Status::OK));

  auto result = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_TRUE(result.empty());
}

/// @test Verify that app_profile_id is set when passed to Table() constructor.
TEST_F(NoexTableTest, Apply_AppProfileId) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  std::string expected_id = "test-id";
  auto mock = MockRpcFactory<btproto::MutateRowRequest,
                             btproto::MutateRowResponse>::Create(expected_id);
  EXPECT_CALL(*client_, MutateRow(_, _, _)).WillOnce(Invoke(mock));

  bigtable::AppProfileId app_profile_id("test-id");
  bigtable::noex::Table table =
      bigtable::noex::Table(client_, app_profile_id, kTableId);
  auto result = table.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_TRUE(result.empty());
}

/// @test Verify that Table::Apply() raises an exception on permanent failures.
TEST_F(NoexTableTest, ApplyFailure) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _))
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
TEST_F(NoexTableTest, ApplyRetry) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _))
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
TEST_F(NoexTableTest, ApplyRetryIdempotent) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
  auto result = table_.Apply(bigtable::SingleRowMutation(
      "not-idempotent", {bigtable::SetCell("fam", "col", "val")}));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(1UL, result.size());
  EXPECT_FALSE(result.front().status().ok());
}

/// @test Verify that Table::BulkApply() works in the easy case.
TEST_F(NoexTableTest, BulkApplySimple) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto reader = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));
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

/// @test Verify that Table::BulkApply() uses app_profile_id when set.
TEST_F(NoexTableTest, BulkApply_AppProfileId) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto reader = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  std::string expected_id = "test-id";
  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(
          Invoke([&reader, expected_id](grpc::ClientContext* ctx,
                                        btproto::MutateRowsRequest const& req) {
            EXPECT_EQ(expected_id, req.app_profile_id());
            return reader.release()->AsUniqueMocked();
          }));
  grpc::Status status;
  bigtable::AppProfileId app_profile_id("test-id");
  bigtable::noex::Table table =
      bigtable::noex::Table(client_, app_profile_id, kTableId);
  table.BulkApply(
      bt::BulkMutation(bt::SingleRowMutation(
                           "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                       bt::SingleRowMutation(
                           "bar", {bt::SetCell("fam", "col", 0_ms, "qux")})),
      status);
  EXPECT_TRUE(status.ok());
  SUCCEED();
}

/// @test Verify that Table::BulkApply() retries partial failures.
TEST_F(NoexTableTest, BulkApplyRetryPartialFailure) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto r1 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial (recoverable) failure.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::StatusCode::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));
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
TEST_F(NoexTableTest, BulkApplyPermanentFailure) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto r1 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::OUT_OF_RANGE);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()));
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
TEST_F(NoexTableTest, BulkApplyCanceledStream) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  // Simulate a stream that returns one success and then terminates.  We expect
  // the BulkApply() operation to retry the request, because the mutation is in
  // an undetermined state.  Well, it should retry assuming it is idempotent,
  // which happens to be the case in this test.
  auto r1 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // Create a second stream returned by the mocks when the client retries.
  auto r2 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));
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
TEST_F(NoexTableTest, BulkApplyTooManyFailures) {
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
      bt::ExponentialBackoffPolicy(10_us, 40_us));

  // Setup the mocks to fail more than 3 times.
  auto r1 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "yikes")));

  auto create_cancelled_stream = [&](grpc::ClientContext*,
                                     btproto::MutateRowsRequest const&) {
    auto stream = bigtable::internal::make_unique<MockMutateRowsReader>();
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "yikes")));
    return stream.release()->AsUniqueMocked();
  };

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
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
  EXPECT_THAT(status.error_message(), HasSubstr("yikes"));
}

/// @test Verify that Table::BulkApply() retries only idempotent mutations.
TEST_F(NoexTableTest, BulkApplyRetryOnlyIdempotent) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  // We will send both idempotent and non-idempotent mutations.  We prepare the
  // mocks to return an empty stream in the first RPC request.  That will force
  // the client to only retry the idempotent mutations.
  auto r1 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));
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
TEST_F(NoexTableTest, BulkApplyFailedRPC) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;

  auto reader = bigtable::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*reader, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                    "no such table")));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));
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
TEST_F(NoexTableTest, CheckAndMutateRowSimple) {
  using namespace ::testing;

  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _))
      .WillOnce(Return(grpc::Status::OK));
  grpc::Status status;
  table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}, status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify that app_profile_id is set when passed to Table() constructor.
TEST_F(NoexTableTest, CheckAndMutateRow_AppProfileId) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  std::string expected_id = "test-id";
  auto mock =
      MockRpcFactory<btproto::CheckAndMutateRowRequest,
                     btproto::CheckAndMutateRowResponse>::Create(expected_id);
  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _)).WillOnce(Invoke(mock));

  bigtable::AppProfileId app_profile_id("test-id");
  bigtable::noex::Table table =
      bigtable::noex::Table(client_, app_profile_id, kTableId);
  grpc::Status status;
  table.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}, status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify that Table::CheckAndMutateRow() raises an on failures.
TEST_F(NoexTableTest, CheckAndMutateRowFailure) {
  using namespace ::testing;

  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));
  grpc::Status status;
  table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}, status);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("try-again"));
}

/// @test Verify that Table::SampleRows<T>() works for default parameter.
TEST_F(NoexTableTest, SampleRowsDefaultParameterTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = bigtable::internal::make_unique<MockSampleRowKeysReader>();
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
  EXPECT_CALL(*client_, SampleRowKeys(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));

  grpc::Status status;
  std::vector<bigtable::RowKeySample> result = table_.SampleRows<>(status);
  EXPECT_TRUE(status.ok());
  auto it = result.begin();
  EXPECT_NE(it, result.end());
  EXPECT_EQ(it->row_key, "test1");
  EXPECT_EQ(it->offset_bytes, 11);
  EXPECT_EQ(++it, result.end());
}

/// @test Verify that app_profile_id is set when passed to Table() constructor.
TEST_F(NoexTableTest, SampleRowKeys_AppProfileId) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  std::string expected_id = "test-id";
  auto reader = bigtable::internal::make_unique<MockSampleRowKeysReader>();
  EXPECT_CALL(*client_, SampleRowKeys(_, _))
      .WillOnce(
          Invoke([expected_id, &reader](grpc::ClientContext* ctx,
                                        btproto::SampleRowKeysRequest request) {
            EXPECT_EQ(expected_id, request.app_profile_id());
            return reader.release()->AsUniqueMocked();
          }));

  EXPECT_CALL(*reader, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  bigtable::AppProfileId app_profile_id("test-id");
  bigtable::noex::Table table =
      bigtable::noex::Table(client_, app_profile_id, kTableId);
  grpc::Status status;
  table.SampleRows<>(status);
  EXPECT_TRUE(status.ok());
}

/// @test Verify that Table::SampleRows<T>() works for std::vector.
TEST_F(NoexTableTest, SampleRowsSimpleVectorTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = bigtable::internal::make_unique<MockSampleRowKeysReader>();
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
  EXPECT_CALL(*client_, SampleRowKeys(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));

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
TEST_F(NoexTableTest, SampleRowsSimpleListTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = bigtable::internal::make_unique<MockSampleRowKeysReader>();
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
  EXPECT_CALL(*client_, SampleRowKeys(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));

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

TEST_F(NoexTableTest, SampleRowsSampleRowKeysRetryTest) {
  using namespace ::testing;
  namespace btproto = ::google::bigtable::v2;

  auto reader = bigtable::internal::make_unique<MockSampleRowKeysReader>();
  auto reader_retry =
      bigtable::internal::make_unique<MockSampleRowKeysReader>();

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

  EXPECT_CALL(*client_, SampleRowKeys(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()))
      .WillOnce(Invoke(reader_retry.release()->MakeMockReturner()));

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

/// @test Verify that Table::SampleRows<T>() reports correctly on too many
/// errors.
TEST_F(NoexTableTest, SampleRowsTooManyFailures) {
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
  auto r1 = bigtable::internal::make_unique<MockSampleRowKeysReader>();
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
    auto stream = new MockSampleRowKeysReader;
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream->AsUniqueMocked();
  };

  EXPECT_CALL(*client_, SampleRowKeys(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(create_cancelled_stream))
      .WillOnce(Invoke(create_cancelled_stream));
  grpc::Status status;
  custom_table.SampleRows<std::vector>(status);
  EXPECT_FALSE(status.ok());
}
