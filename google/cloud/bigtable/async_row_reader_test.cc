// Copyright 2019 Google LLC
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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_data_client.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = google::bigtable::v2;
using namespace ::testing;
using namespace google::cloud::testing_util::chrono_literals;
using bigtable::testing::MockClientAsyncReaderInterface;

class TableAsyncReadRowsTest : public bigtable::testing::TableTestFixture {
 protected:
  TableAsyncReadRowsTest()
      : cq_impl_(new bigtable::testing::MockCompletionQueue), cq_(cq_impl_) {}

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>& AddReader(
      std::function<void(btproto::ReadRowsRequest const&)>
          request_expectations) {
    readers_.emplace_back(
        new MockClientAsyncReaderInterface<btproto::ReadRowsResponse>);
    reader_started_.push_back(false);
    size_t idx = reader_started_.size() - 1;
    auto& reader = LastReader();
    // We can't move request_expectations into the lambda because of lack of
    // generalized lambda capture in C++11, so let's pass it by pointer.
    auto request_expectations_ptr =
        std::make_shared<decltype(request_expectations)>(
            std::move(request_expectations));

    EXPECT_CALL(*client_, PrepareAsyncReadRows(_, _, _))
        .WillOnce(
            Invoke([&reader, request_expectations_ptr](
                       grpc::ClientContext*, btproto::ReadRowsRequest const& r,
                       grpc::CompletionQueue*) {
              (*request_expectations_ptr)(r);
              return std::unique_ptr<
                  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>(
                  &reader);
            }))
        .RetiresOnSaturation();

    EXPECT_CALL(reader, StartCall(_)).WillOnce(Invoke([idx, this](void*) {
      reader_started_[idx] = true;
    }));
    // The last call, to which we'll return ok==false.
    EXPECT_CALL(reader, Read(_, _))
        .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {}));
    return reader;
  }

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>& LastReader() {
    return *readers_.back();
  }

  std::shared_ptr<bigtable::testing::MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::vector<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>*>
      readers_;
  // Whether `Start()` was called on a reader at the respective index.
  std::vector<bool> reader_started_;
};

/// @test Verify that Table::AsyncReadRows() works in a simple case.
TEST_F(TableAsyncReadRowsTest, ResponseAfterRowIsAskedFor) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  auto row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()

  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data
  auto row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r1", (*row)->row_key());

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());

  // All following calls will keep returning EOF.
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

TEST_F(TableAsyncReadRowsTest, ResponseBeforeRowIsAskedFor) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  EXPECT_TRUE(reader_started_[0]);
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  auto row_future = reader->Next();
  auto row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r1", (*row)->row_key());

  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

TEST_F(TableAsyncReadRowsTest, MultipleFuturesSatisfiedAtOnce) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
chunks {
row_key: "r2"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
chunks {
row_key: "r3"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  EXPECT_TRUE(reader_started_[0]);
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  auto row_future = reader->Next();
  auto row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r1", (*row)->row_key());

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  row_future = reader->Next();
  row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r2", (*row)->row_key());

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  row_future = reader->Next();
  row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r3", (*row)->row_key());

  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

TEST_F(TableAsyncReadRowsTest, ResponseInMultipleChunks) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: false
}
)");
      }))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col2" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  EXPECT_TRUE(reader_started_[0]);
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()

  auto row_future = reader->Next();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data

  auto row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r1", (*row)->row_key());
  ASSERT_EQ(2, (*row)->cells().size());

  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

TEST_F(TableAsyncReadRowsTest, ParserEofFailsOnUnfinishedRow) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            // missing final commit
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: false
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data
  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  auto row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  auto row = row_future.get();

  ASSERT_FALSE(row);

  Status final_status = row.status();

  // All following calls will keep returning the same status.
  row = reader->Next().get();
  ASSERT_FALSE(row);
  ASSERT_EQ(final_status, row.status());
  row = reader->Next().get();
  ASSERT_FALSE(row);
  ASSERT_EQ(final_status, row.status());
}

TEST_F(TableAsyncReadRowsTest, ParserEofDoesntFailsOnUnfinishedRowIfRowLimit) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            // missing final commit
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
chunks {
row_key: "r2"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: false
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), 1, Filter::PassAllFilter());

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data
  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  auto row_future = reader->Next();
  auto row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r1", (*row)->row_key());

  ASSERT_EQ(0U, cq_impl_->size());
  row_future = reader->Next();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());

  // All following calls will keep returning the same status.
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

TEST_F(TableAsyncReadRowsTest, PermanentFailure) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "noooo");
      }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  auto row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()

  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  auto row = row_future.get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kPermissionDenied, row.status().code());

  // All following calls will keep returning EOF.
  row = reader->Next().get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kPermissionDenied, row.status().code());
  row = reader->Next().get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kPermissionDenied, row.status().code());
}

TEST_F(TableAsyncReadRowsTest, TransientErrorIsRetried) {
  auto& stream2 = AddReader([](btproto::ReadRowsRequest const& req) {
    // Verify that we're not asking for the same rows again.
    EXPECT_TRUE(req.has_rows());
    auto const& rows = req.rows();
    EXPECT_EQ(1U, rows.row_ranges_size());
    auto const& range = rows.row_ranges(0);
    EXPECT_EQ("r1", range.start_key_open());
  });
  auto& stream1 = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream1, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
chunks {
row_key: "r2"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: false
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream1, Finish(_, _))
      .WillOnce(Invoke([](grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "oh no");
      }));

  EXPECT_CALL(stream2, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r2"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream2, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data

  auto row_future = reader->Next();
  auto row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r1", (*row)->row_key());

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream with failure
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish timer
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));
  cq_impl_->SimulateCompletion(cq_, true);  // Return data

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r2", (*row)->row_key());

  ASSERT_EQ(0U, cq_impl_->size());
  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());

  // All following calls will keep returning EOF.
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

TEST_F(TableAsyncReadRowsTest, PerserFailure) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            // Row not in increasing order.
            R"(
chunks {
row_key: "r2"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  auto row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()

  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data
  auto row = row_future.get();

  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->has_value());
  ASSERT_EQ("r2", (*row)->row_key());

  // Check that we started finishing the stream.
  ASSERT_EQ(1U, cq_impl_->size());
  row_future = reader->Next();
  EXPECT_EQ(std::future_status::timeout, row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish dummy Read()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  row = row_future.get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kInternal, row.status().code());

  // All following calls will keep returning EOF.
  row = reader->Next().get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kInternal, row.status().code());
  row = reader->Next().get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kInternal, row.status().code());
}

TEST_F(TableAsyncReadRowsTest, VerifySubmittingFromCallbacksWorks) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const& req) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r1"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
chunks {
row_key: "r2"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .WillOnce(Invoke([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
chunks {
row_key: "r3"
    family_name { value: "fam" }
    qualifier { value: "col" }
timestamp_micros: 42000
value: "value"
commit_row: true
}
)");
      }))
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _))
      .WillOnce(Invoke(
          [](grpc::Status* status, void*) { *status = grpc::Status::OK; }));

  auto reader = table_.AsyncReadRows(cq_, RowSet(), RowReader::NO_ROWS_LIMIT,
                                     Filter::PassAllFilter());
  std::atomic<bool> first_future_completed(false);
  std::atomic<bool> second_future_completed(false);

  auto row_future = reader->Next();
  auto second_row_future =
      row_future.then([this, &reader, &first_future_completed](
                          future<StatusOr<optional<Row>>> fut) {
        auto row = fut.get();
        EXPECT_STATUS_OK(row);
        EXPECT_TRUE(row->has_value());
        EXPECT_EQ("r1", (*row)->row_key());
        first_future_completed = true;
        size_t size_before_submitting = cq_impl_->size();
        auto res = reader->Next();
        // Verify this doesn't submit any new requests.
        EXPECT_EQ(size_before_submitting, cq_impl_->size());
        return res;
      });
  auto third_row_future =
      second_row_future.then([this, &second_future_completed,
                              &reader](future<StatusOr<optional<Row>>> fut) {
        auto row = fut.get();
        EXPECT_STATUS_OK(row);
        EXPECT_TRUE(row->has_value());
        EXPECT_EQ("r2", (*row)->row_key());
        second_future_completed = true;
        size_t size_before_submitting = cq_impl_->size();
        auto res = reader->Next();
        // Verify this doesn't submit any new requests.
        EXPECT_EQ(size_before_submitting, cq_impl_->size());
        return res;
      });

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Start()

  ASSERT_EQ(1U, cq_impl_->size());
  ASSERT_FALSE(first_future_completed);
  cq_impl_->SimulateCompletion(cq_, true);  // Return data
  ASSERT_TRUE(first_future_completed);
  ASSERT_TRUE(second_future_completed);
  EXPECT_EQ(std::future_status::timeout, third_row_future.wait_for(1_ms));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Return data

  auto third_row = third_row_future.get();
  ASSERT_STATUS_OK(third_row);
  ASSERT_EQ("r3", (*third_row)->row_key());

  row_future = reader->Next();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(cq_, true);  // Finish Finish()

  auto row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());

  // All following calls will keep returning EOF.
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
  row = reader->Next().get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->has_value());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
