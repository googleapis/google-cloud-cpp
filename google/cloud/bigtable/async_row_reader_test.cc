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
#include "google/cloud/bigtable/testing/mock_data_client.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/bigtable/testing/validate_metadata.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/mock_completion_queue.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = google::bigtable::v2;

using ::google::cloud::bigtable::testing::MockClientAsyncReaderInterface;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::MockCompletionQueue;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Values;
using ::testing::WithParamInterface;

template <typename T>
bool Unsatisfied(future<T> const& fut) {
  return std::future_status::timeout == fut.wait_for(1_ms);
}

class TableAsyncReadRowsTest : public bigtable::testing::TableTestFixture {
 protected:
  TableAsyncReadRowsTest()
      : cq_impl_(new MockCompletionQueue),
        cq_(cq_impl_),
        stream_status_future_(stream_status_promise_.get_future()) {}

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>& AddReader(
      std::function<void(btproto::ReadRowsRequest const&)> request_expectations,
      bool expect_a_read = true) {
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
        .WillOnce([&reader, request_expectations_ptr](
                      grpc::ClientContext* context,
                      btproto::ReadRowsRequest const& r,
                      grpc::CompletionQueue*) {
          EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
              *context, "google.bigtable.v2.Bigtable.ReadRows"));
          (*request_expectations_ptr)(r);
          return std::unique_ptr<
              MockClientAsyncReaderInterface<btproto::ReadRowsResponse>>(
              &reader);
        })
        .RetiresOnSaturation();

    EXPECT_CALL(reader, StartCall(_)).WillOnce([idx, this](void*) {
      reader_started_[idx] = true;
    });
    if (expect_a_read) {
      // The last call, to which we'll return ok==false.
      EXPECT_CALL(reader, Read(_, _))
          .WillOnce([](btproto::ReadRowsResponse*, void*) {});
    }
    return reader;
  }

  MockClientAsyncReaderInterface<btproto::ReadRowsResponse>& LastReader() {
    return *readers_.back();
  }

  // Start Table::AsyncReadRows.
  void ReadRows(int row_limit = RowReader::NO_ROWS_LIMIT) {
    table_.AsyncReadRows(
        cq_,
        [this](Row const& row) {
          EXPECT_EQ(expected_rows_.front(), row.row_key());
          expected_rows_.pop();
          row_promises_.front().set_value(row.row_key());
          row_promises_.pop();
          auto ret = std::move(futures_from_user_cb_.front());
          futures_from_user_cb_.pop();
          return ret;
        },
        [this](Status const& stream_status) {
          stream_status_promise_.set_value(stream_status);
        },
        RowSet(), row_limit, Filter::PassAllFilter());
  }

  /// Expect a row whose row key is equal to this function's argument.
  template <typename T>
  void ExpectRow(T const& row) {
    row_promises_.emplace(promise<RowKeyType>());
    row_futures_.emplace_back(row_promises_.back().get_future());
    promises_from_user_cb_.emplace_back(promise<bool>());
    futures_from_user_cb_.emplace(promises_from_user_cb_.back().get_future());
    expected_rows_.push(RowKeyType(row));
  }

  /// A wrapper around ExpectRow to expect many rows.
  template <typename T>
  void ExpectRows(std::initializer_list<T> const& rows) {
    for (auto const& row : rows) {
      ExpectRow(row);
    }
  }

  std::shared_ptr<MockCompletionQueue> cq_impl_;
  bigtable::CompletionQueue cq_;
  std::vector<MockClientAsyncReaderInterface<btproto::ReadRowsResponse>*>
      readers_;
  // Whether `Start()` was called on i-th retry attempt.
  std::vector<bool> reader_started_;
  std::queue<promise<RowKeyType>> row_promises_;
  /**
   * Future at idx i corresponse to i-th expected row. It will be satisfied
   * when the relevant `on_row` callback of AsyncReadRows is called.
   */
  std::vector<future<RowKeyType>> row_futures_;
  std::queue<RowKeyType> expected_rows_;
  promise<Status> stream_status_promise_;
  /// Future which will be satisfied with the status passed in on_finished.
  future<Status> stream_status_future_;
  /// I-th promise corresponds to the future returned from the ith on_row cb.
  std::vector<promise<bool>> promises_from_user_cb_;
  std::queue<future<bool>> futures_from_user_cb_;
};

/// @test Verify that successfully reading a sinle row works.
TEST_F(TableAsyncReadRowsTest, SingleRow) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r1"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  auto row = row_futures_[0].get();

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  promises_from_user_cb_[0].set_value(true);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Like SingleRow, but the future returned from the cb is satisfied.
TEST_F(TableAsyncReadRowsTest, SingleRowInstantFinish) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r1"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  promises_from_user_cb_[0].set_value(true);
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  auto row = row_futures_[0].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Verify that reading 2 rows delivered in 2 responses works.
TEST_F(TableAsyncReadRowsTest, MultipleChunks) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r1"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r2"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  ExpectRow("r2");
  promises_from_user_cb_[1].set_value(true);
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  row_futures_[0].get();

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  promises_from_user_cb_[0].set_value(true);

  EXPECT_TRUE(Unsatisfied(row_futures_[1]));
  cq_impl_->SimulateCompletion(true);  // Return data
  row_futures_[1].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Like MultipleChunks but the future returned from on_row is satisfied.
TEST_F(TableAsyncReadRowsTest, MultipleChunksImmediatelySatisfied) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r1"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r2"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  ExpectRow("r2");
  promises_from_user_cb_[0].set_value(true);
  promises_from_user_cb_[1].set_value(true);
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  row_futures_[0].get();

  EXPECT_TRUE(Unsatisfied(row_futures_[1]));
  cq_impl_->SimulateCompletion(true);  // Return data
  row_futures_[1].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Verify that a single row can span mutiple responses.
TEST_F(TableAsyncReadRowsTest, ResponseInMultipleChunks) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r1"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: false
                })");
      })
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r1"
                  family_name { value: "fam" }
                  qualifier { value: "col2" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  promises_from_user_cb_[0].set_value(true);
  ReadRows();
  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  EXPECT_TRUE(Unsatisfied(row_futures_[0]));
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data

  row_futures_[0].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Verify that parser fails if the stream finishes prematurely.
TEST_F(TableAsyncReadRowsTest, ParserEofFailsOnUnfinishedRow) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
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
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  ASSERT_FALSE(stream_status_future_.get().ok());
}

/// @test Check that we ignore HandleEndOfStream errors if enough rows were read
TEST_F(TableAsyncReadRowsTest, ParserEofDoesntFailsOnUnfinishedRowIfRowLimit) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
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
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  promises_from_user_cb_[0].set_value(true);
  ReadRows(1);

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(row_futures_[0]));
  cq_impl_->SimulateCompletion(true);  // Return data

  row_futures_[0].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Verify that permanent errors are not retried and properly passed.
TEST_F(TableAsyncReadRowsTest, PermanentFailure) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "noooo");
  });

  ReadRows();
  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_EQ(StatusCode::kPermissionDenied, stream_status.code());
}

/// @test Verify that transient errors are retried.
TEST_F(TableAsyncReadRowsTest, TransientErrorIsRetried) {
  auto& stream2 = AddReader([](btproto::ReadRowsRequest const& req) {
    // Verify that we're not asking for the same rows again.
    EXPECT_TRUE(req.has_rows());
    auto const& rows = req.rows();
    EXPECT_EQ(1, rows.row_ranges_size());
    auto const& range = rows.row_ranges(0);
    EXPECT_EQ("r1", range.start_key_open());
  });
  auto& stream1 = AddReader([](btproto::ReadRowsRequest const&) {});

  // Make it a bit trickier by delivering the error while parsing second row.
  EXPECT_CALL(stream1, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
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
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream1, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "oh no");
  });

  EXPECT_CALL(stream2, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
                chunks {
                  row_key: "r2"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream2, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRows({"r1", "r2"});
  promises_from_user_cb_[0].set_value(true);
  promises_from_user_cb_[1].set_value(true);
  ReadRows();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data

  row_futures_[0].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream with failure
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish timer
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(row_futures_[1]));
  cq_impl_->SimulateCompletion(true);  // Return data

  row_futures_[1].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

/// @test Verify proper handling of bogus responses from the service.
TEST_F(TableAsyncReadRowsTest, ParserFailure) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
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
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r2");
  promises_from_user_cb_[0].set_value(true);
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish dummy Read()
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  row_futures_[0].get();

  auto stream_status = stream_status_future_.get();
  ASSERT_EQ(StatusCode::kInternal, stream_status.code());
  ASSERT_EQ(0U, cq_impl_->size());
}

enum class CancelMode {
  kFalseValue,
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  kStdExcept,
  kOtherExcept,
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
};

/// @test Verify canceling the stream by satisfying the futures with false
class TableAsyncReadRowsCancelMidStreamTest
    : public TableAsyncReadRowsTest,
      public WithParamInterface<CancelMode> {};

TEST_P(TableAsyncReadRowsCancelMidStreamTest, CancelMidStream) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
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
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data
  row_futures_[0].get();

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());

  switch (GetParam()) {
    case CancelMode::kFalseValue:
      promises_from_user_cb_[0].set_value(false);
      break;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    case CancelMode::kStdExcept:
      try {
        throw std::runtime_error("user threw std::exception");
      } catch (...) {
        promises_from_user_cb_[0].set_exception(std::current_exception());
      }
      break;
    case CancelMode::kOtherExcept:
      try {
        throw 5;
      } catch (...) {
        promises_from_user_cb_[0].set_exception(std::current_exception());
      }
      break;
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  }

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_EQ(StatusCode::kCancelled, stream_status.code());
  switch (GetParam()) {
    case CancelMode::kFalseValue:
      ASSERT_THAT(stream_status.message(), HasSubstr("User cancelled"));
      break;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    case CancelMode::kStdExcept:
      ASSERT_THAT(stream_status.message(),
                  HasSubstr("user threw std::exception"));
      break;
    case CancelMode::kOtherExcept:
      ASSERT_THAT(stream_status.message(), HasSubstr("unknown exception"));
      break;
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  }

  ASSERT_EQ(0U, cq_impl_->size());
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
INSTANTIATE_TEST_SUITE_P(CancelMidStream, TableAsyncReadRowsCancelMidStreamTest,
                         Values(CancelMode::kFalseValue, CancelMode::kStdExcept,
                                CancelMode::kOtherExcept));
#else   // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
INSTANTIATE_TEST_SUITE_P(CancelMidStream, TableAsyncReadRowsCancelMidStreamTest,
                         Values(CancelMode::kFalseValue));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Like CancelMidStream but after the underlying stream has finished.
TEST_F(TableAsyncReadRowsTest, CancelAfterStreamFinish) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  // First two rows are going to be processed, but third will cause the parser
  // to fail (row order violation). This will result in finishing the stream
  // while still keeping the two processed rows for the user.
  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
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
                  row_key: "r0"
                  family_name { value: "fam" }
                  qualifier { value: "col" }
                  timestamp_micros: 42000
                  value: "value"
                  commit_row: true
                })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  ExpectRow("r1");
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(row_futures_[0]));
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()
  ASSERT_EQ(0U, cq_impl_->size());

  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  auto row = row_futures_[0].get();

  // Check that we're not asking for data unless someone is waiting for it.
  ASSERT_EQ(0U, cq_impl_->size());
  promises_from_user_cb_[0].set_value(false);

  auto stream_status = stream_status_future_.get();
  ASSERT_FALSE(stream_status.ok());
  ASSERT_EQ(StatusCode::kCancelled, stream_status.code());
}

/// @test Verify that the recursion described in TryGiveRowToUser is bounded.
TEST_F(TableAsyncReadRowsTest, DeepStack) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  auto large_response = bigtable::testing::ReadRowsResponseFromString(
      R"(
          chunks {
            row_key: "000"
            family_name { value: "fam" }
            qualifier { value: "col" }
            timestamp_micros: 42000
            value: "value"
            commit_row: true
          })");
  ExpectRow("000");
  for (int i = 1; i < 101; ++i) {
    auto chunk = large_response.chunks(0);
    std::stringstream s_idx;
    s_idx << std::setfill('0') << std::setw(3) << i;
    chunk.set_row_key(s_idx.str());
    ExpectRow(chunk.row_key());
    *large_response.add_chunks() = std::move(chunk);
  }

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([large_response](btproto::ReadRowsResponse* r, void*) {
        *r = large_response;
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  for (int i = 0; i < 101; ++i) {
    promises_from_user_cb_[i].set_value(true);
  }
  ReadRows();

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_futures_[0]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data

  for (int i = 0; i < 100; ++i) {
    row_futures_[i].get();
  }
  ASSERT_TRUE(Unsatisfied(row_futures_[100]));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // RunAsync
  row_futures_[100].get();

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(stream_status_future_));
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto stream_status = stream_status_future_.get();
  ASSERT_STATUS_OK(stream_status);
  ASSERT_EQ(0U, cq_impl_->size());
}

TEST_F(TableAsyncReadRowsTest, ReadRowSuccess) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Read(_, _))
      .WillOnce([](btproto::ReadRowsResponse* r, void*) {
        *r = bigtable::testing::ReadRowsResponseFromString(
            R"(
              chunks {
                row_key: "000"
                family_name { value: "fam" }
                qualifier { value: "col" }
                timestamp_micros: 42000
                value: "value"
                commit_row: true
              })");
      })
      .RetiresOnSaturation();
  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  auto row_future = table_.AsyncReadRow(cq_, "000", Filter::PassAllFilter());

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  EXPECT_TRUE(Unsatisfied(row_future));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Return data

  // We return data only after the whole stream is finished.
  ASSERT_TRUE(Unsatisfied(row_future));

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_TRUE(row->first);
  ASSERT_EQ("000", row->second.row_key());

  ASSERT_EQ(0U, cq_impl_->size());
}

TEST_F(TableAsyncReadRowsTest, ReadRowNotFound) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status::OK;
  });

  auto row_future = table_.AsyncReadRow(cq_, "000", Filter::PassAllFilter());

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(row_future));

  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto row = row_future.get();
  ASSERT_STATUS_OK(row);
  ASSERT_FALSE(row->first);
}

TEST_F(TableAsyncReadRowsTest, ReadRowError) {
  auto& stream = AddReader([](btproto::ReadRowsRequest const&) {});

  EXPECT_CALL(stream, Finish(_, _)).WillOnce([](grpc::Status* status, void*) {
    *status = grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "");
  });

  auto row_future = table_.AsyncReadRow(cq_, "000", Filter::PassAllFilter());

  EXPECT_TRUE(reader_started_[0]);

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(true);  // Finish Start()

  ASSERT_EQ(1U, cq_impl_->size());
  cq_impl_->SimulateCompletion(false);  // Finish stream
  ASSERT_EQ(1U, cq_impl_->size());
  EXPECT_TRUE(Unsatisfied(row_future));

  cq_impl_->SimulateCompletion(true);  // Finish Finish()

  auto row = row_future.get();
  ASSERT_FALSE(row);
  ASSERT_EQ(StatusCode::kPermissionDenied, row.status().code());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
