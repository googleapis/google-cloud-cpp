// Copyright 2018 Google LLC.
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

#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/testing/internal_table_test_fixture.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_data_client.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
namespace {

namespace bt = ::google::cloud::bigtable;
namespace btproto = google::bigtable::v2;
using namespace ::testing;

class NoexTableAsyncReadRowsTest
    : public bigtable::testing::internal::TableTestFixture {};

/// @test Verify that noex::Table::AsyncReadRows() works in a simple case.
TEST_F(NoexTableAsyncReadRowsTest, Simple) {
  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::ReadRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>
      reader_deleter1(reader1);

  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        {
          auto c = r->add_chunks();
          c->set_row_key("0001");
          c->set_timestamp_micros(1000);
          c->set_value("test");
          c->set_value_size(0);
          c->set_commit_row(true);
        }
      }))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {}));

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncReadRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::ReadRowsRequest const& r,
                                          grpc::CompletionQueue*, void*) {
        return std::move(reader_deleter1);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool read_rows_op_called = false;
  bool done_op_called = false;

  table_.AsyncReadRows(cq,
                       [&read_rows_op_called](CompletionQueue& cq, Row row,
                                              grpc::Status& status) {
                         EXPECT_EQ("0001", row.row_key());
                         EXPECT_TRUE(status.ok());
                         read_rows_op_called = true;
                       },
                       [&done_op_called](CompletionQueue& cq, bool& response,
                                         grpc::Status const& status) {
                         EXPECT_TRUE(response);
                         EXPECT_TRUE(status.ok());
                         EXPECT_EQ("mocked-status", status.error_message());
                         done_op_called = true;
                       },
                       bt::RowSet(), bt::RowReader::NO_ROWS_LIMIT,
                       bt::Filter::PassAllFilter());

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  EXPECT_FALSE(read_rows_op_called);
  impl->SimulateCompletion(cq, true);
  EXPECT_TRUE(read_rows_op_called);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(done_op_called);
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(done_op_called);
}

/// @test Verify that noex::Table::AsyncReadRows() works for retry scenario.
TEST_F(NoexTableAsyncReadRowsTest, ReadRowsWithRetry) {
  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::ReadRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>
      reader_deleter1(reader1);

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>* reader2 =
      new MockClientAsyncReaderInterface<btproto::ReadRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>
      reader_deleter2(reader2);

  // first attempt to AsyncReadRows will fail after returning one row
  // and results in second call to AsyncReadRows.
  EXPECT_CALL(*reader1, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        {
          auto c = r->add_chunks();
          c->set_row_key("0001");
          c->set_timestamp_micros(1000);
          c->set_value("test-0001");
          c->set_value_size(0);
          c->set_commit_row(true);
        }
      }))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {}));

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "mocked-status");
      }));

  EXPECT_CALL(*reader2, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        {
          auto c = r->add_chunks();
          c->set_row_key("0002");
          c->set_timestamp_micros(1000);
          c->set_value("test-0002");
          c->set_value_size(0);
          c->set_commit_row(true);
        }
      }))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {}))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {}));

  EXPECT_CALL(*reader2, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncReadRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::ReadRowsRequest const& r,
                                          grpc::CompletionQueue*, void*) {
        EXPECT_EQ("0000", r.rows().row_ranges(0).start_key_closed());
        EXPECT_EQ("0005", r.rows().row_ranges(0).end_key_open());
        return std::move(reader_deleter1);
      }))
      .WillOnce(Invoke([&reader_deleter2](grpc::ClientContext*,
                                          btproto::ReadRowsRequest const& r,
                                          grpc::CompletionQueue*, void*) {
        // second attempt will request the rows that have not been returned.
        EXPECT_EQ("0001", r.rows().row_ranges(0).start_key_open());
        EXPECT_EQ("0005", r.rows().row_ranges(0).end_key_open());
        return std::move(reader_deleter2);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool read_rows_op_called = false;
  bool done_op_called = false;

  table_.AsyncReadRows(cq,
                       [&read_rows_op_called](CompletionQueue& cq, Row row,
                                              grpc::Status& status) {
                         EXPECT_TRUE(status.ok());
                         read_rows_op_called = true;
                       },
                       [&done_op_called](CompletionQueue& cq, bool& response,
                                         grpc::Status const& status) {
                         EXPECT_TRUE(response);
                         EXPECT_TRUE(status.ok());
                         EXPECT_EQ("mocked-status", status.error_message());
                         done_op_called = true;
                       },
                       bt::RowSet(bt::RowRange::Range("0000", "0005")),
                       bt::RowReader::NO_ROWS_LIMIT,
                       bt::Filter::PassAllFilter());

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  impl->SimulateCompletion(cq, true);
  // finished, scheduled timer
  impl->SimulateCompletion(cq, true);
  // timer finished, retry
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 1 read
  impl->SimulateCompletion(cq, true);
  // state == PROCESSING, 2 read
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(done_op_called);
  impl->SimulateCompletion(cq, true);
  EXPECT_TRUE(done_op_called);
}

/// @test Verify that noex::Table::AsyncReadRows() works when cancelled
TEST_F(NoexTableAsyncReadRowsTest, Cancelled) {
  // This test attempts to read row but fails straight away because
  // the user cancels the request.

  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::ReadRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>
      reader_deleter1(reader1);

  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::CANCELLED, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncReadRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::ReadRowsRequest const&,
                                          grpc::CompletionQueue*, void*) {
        return std::move(reader_deleter1);
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  bool read_rows_op_called = false;
  bool done_op_called = false;

  table_.AsyncReadRows(
      cq,
      [&read_rows_op_called](CompletionQueue& cq, Row row,
                             grpc::Status& status) {
        read_rows_op_called = true;
      },
      [&done_op_called](CompletionQueue& cq, bool& response,
                        grpc::Status const& status) {
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(grpc::StatusCode::CANCELLED, status.error_code());
        done_op_called = true;
      },
      bt::RowSet(), bt::RowReader::NO_ROWS_LIMIT, bt::Filter::PassAllFilter());

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(read_rows_op_called);
  EXPECT_FALSE(done_op_called);
  impl->SimulateCompletion(cq, false);
  // callback fired
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(done_op_called);
  EXPECT_FALSE(read_rows_op_called);
}

/// @test Verify that noex::Table::AsyncReadRows() works when permanent error
//  occurs
TEST_F(NoexTableAsyncReadRowsTest, PermanentError) {
  using bigtable::testing::MockClientAsyncReaderInterface;

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>* reader1 =
      new MockClientAsyncReaderInterface<btproto::ReadRowsResponse>;
  std::unique_ptr<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>
      reader_deleter1(reader1);
  EXPECT_CALL(*reader1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status =
            grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncReadRows(_, _, _, _))
      .WillOnce(Invoke([&reader_deleter1](grpc::ClientContext*,
                                          btproto::ReadRowsRequest const&,
                                          grpc::CompletionQueue*, void*) {
        return std::move(reader_deleter1);
      }));

  bool read_rows_op_called = false;
  bool done_op_called = false;

  auto policy = bt::DefaultIdempotentMutationPolicy();
  auto impl = std::make_shared<bigtable::testing::MockCompletionQueue>();
  using bigtable::CompletionQueue;
  bigtable::CompletionQueue cq(impl);

  table_.AsyncReadRows(
      cq,
      [&read_rows_op_called](CompletionQueue& cq, Row row,
                             grpc::Status& status) {
        read_rows_op_called = true;
      },
      [&done_op_called](CompletionQueue& cq, bool& response,
                        grpc::Status const& status) {
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(grpc::StatusCode::PERMISSION_DENIED, status.error_code());
        done_op_called = true;
      },
      bt::RowSet(), bt::RowReader::NO_ROWS_LIMIT, bt::Filter::PassAllFilter());

  using bigtable::AsyncOperation;
  impl->SimulateCompletion(cq, false);
  // state == FINISHING
  EXPECT_FALSE(read_rows_op_called);
  EXPECT_FALSE(done_op_called);
  impl->SimulateCompletion(cq, false);
  // callback fired
  impl->SimulateCompletion(cq, false);
  EXPECT_TRUE(done_op_called);
  EXPECT_FALSE(read_rows_op_called);
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
