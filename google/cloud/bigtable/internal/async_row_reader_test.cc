// Copyright 2019 Google LLC
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

#include "google/cloud/bigtable/internal/async_row_reader.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ms = std::chrono::milliseconds;
using ::google::cloud::bigtable::DataLimitedErrorCountRetryPolicy;
using ::google::cloud::bigtable::testing::MockAsyncReadRowsStream;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable_internal::AsyncRowReader;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::Unused;
using ::testing::Values;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try again");
}

// The individual pairs are: {row_key, commit_row}
//
// We use `commit_row == true` to return a full row, and `commit_row == false`
// to return a partial row.
absl::optional<v2::ReadRowsResponse> MakeResponse(
    std::vector<std::pair<std::string, bool>> rows) {
  v2::ReadRowsResponse resp;
  for (auto& row : rows) {
    auto& c = *resp.add_chunks();
    c.set_row_key(std::move(row.first));
    c.mutable_family_name()->set_value("cf");
    c.mutable_qualifier()->set_value("cq");
    c.set_timestamp_micros(42000);
    c.set_value("value");
    c.set_commit_row(row.second);
  }
  return resp;
}

absl::optional<v2::ReadRowsResponse> EndOfStream() { return absl::nullopt; }

/// @test Verify that successfully reading rows works.
TEST(AsyncRowReaderTest, Success) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{"r1", true}, {"r2", true}}));
            })
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r3", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  // We verify that the on_row callback supplied to the `AsyncRowReader` is
  // invoked for each row we expect to receive. We also use it to simulate the
  // input received from the caller, a `future<bool>` that tells us whether to
  // keep reading or not.
  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r2", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r3", row.row_key());
        return make_ready_future(true);
      });

  // We verify that the on_finish callback supplied to the `AsyncRowReader` is
  // invoked with the correct final status for the operation.
  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  // The backoff policy method will be invoked once for every retry.
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  // In order to verify that the current options are used to configure the
  // `grpc::ClientContext` on every stream attempt, we instantiate an
  // OptionsSpan with the `GrpcSetupOption` set.
  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  // Perform the asynchronous streaming read retry loop with the given
  // configuration.
  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

/// @test Verify that reading works when the futures are not immediately
/// satisfied.
TEST(AsyncRowReaderTest, SuccessDelayedFuture) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{"r1", true}, {"r2", true}}));
            })
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r3", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  promise<bool> p1;
  promise<bool> p2;
  promise<bool> p3;
  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([&p1](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(p1.get_future());
      })
      .WillOnce([&p2](bigtable::Row const& row) {
        EXPECT_EQ("r2", row.row_key());
        return make_ready_future(p2.get_future());
      })
      .WillOnce([&p3](bigtable::Row const& row) {
        EXPECT_EQ("r3", row.row_key());
        return make_ready_future(p3.get_future());
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));

  // Satisfy the futures
  p1.set_value(true);
  p2.set_value(true);
  p3.set_value(true);
}

/// @test Verify that a single row can span multiple responses.
TEST(AsyncRowReaderTest, ResponseInMultipleChunks) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r1", false}}));
            })
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r1", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });
  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r1", row.row_key());
    return make_ready_future(true);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

/// @test Verify that parser fails if the stream finishes prematurely.
TEST(AsyncRowReaderTest, ParserEofFailsOnUnfinishedRow) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            // The service returns an unfinished row, then ends the stream. This
            // should yield a kInternal error, which (by default) is not
            // retryable.
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r1", false}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kInternal));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

/// @test Check that we ignore HandleEndOfStream errors if enough rows were read
TEST(AsyncRowReaderTest, ParserEofDoesntFailOnUnfinishedRowIfRowLimit) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              // In this test, the service returns a full row, and an unfinished
              // row. Normally this would cause an error in the parser, but
              // because the caller has only asked for 1 row total, the call
              // succeeds.
              return make_ready_future(
                  MakeResponse({{"r1", true}, {"r2", false}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r1", row.row_key());
    return make_ready_future(true);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet(), 1,
      bigtable::Filter::PassAllFilter(), std::move(retry), std::move(mock_b));
}

/// @test Verify that permanent errors are not retried and properly passed.
TEST(AsyncRowReaderTest, PermanentFailure) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kPermissionDenied, "fail"));
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

TEST(AsyncRowReaderTest, RetryPolicyExhausted) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(kNumRetries)
      .WillRepeatedly([] {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion)
      .Times(kNumRetries)
      .WillRepeatedly(Return(ms(0)));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

/// @test Verify that retries do not ask for rows we have already read.
TEST(AsyncRowReaderTest, RetrySkipsReadRows) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // The initial row set contains two rows: "r1" and "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2"));
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            // The service returns "r1", then fails with a retryable error.
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r1", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      })
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // Because we have already received "r1", we should not ask for it
        // again. The row set for this call should only contain: "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r2"));
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r2", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r2", row.row_key());
        return make_ready_future(true);
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).WillOnce(Return(ms(0)));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry), std::move(mock_b));
}

/// @test Verify that we do not retry at all if the rowset will be empty.
TEST(AsyncRowReaderTest, NoRetryIfRowSetIsEmpty) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // The initial row set contains one row: "r1".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1"));
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            // The service returns "r1", then fails with a retryable error. We
            // do not need to retry, because the row set is now empty. The
            // overall stream should succeed.
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r1", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r1", row.row_key());
    return make_ready_future(true);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry), std::move(mock_b));
}

/// @test Verify that the last scanned row is respected.
TEST(AsyncRowReaderTest, LastScannedRowKeyIsRespected) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // The initial row set contains three rows: "r1", "r2", and "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2", "r3"));
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            // The service will return "r1". But it will also tell us that "r2"
            // has been scanned, before failing with a transient error.
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r1", true}}));
            })
            .WillOnce([] {
              v2::ReadRowsResponse r;
              r.set_last_scanned_row_key("r2");
              return make_ready_future(absl::make_optional(r));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      })
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // Because the service has scanned up to "r2", we should not ask for
        // "r2" again. The row set for this call should only contain: "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r3"));
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r3", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r3", row.row_key());
        return make_ready_future(true);
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).WillOnce(Return(ms(0)));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry), std::move(mock_b));
}

/// @test Verify proper handling of bogus responses from the service.
TEST(AsyncRowReaderTest, ParserFailsOnOutOfOrderRowKeys) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([] {
          // The rows are returned out of order.
          return make_ready_future(MakeResponse({{"r2", true}, {"r1", true}}));
        });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(EndOfStream());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r2", row.row_key());
    return make_ready_future(true);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kInternal));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

/// @test Verify canceling the stream by satisfying the futures with false
class AsyncRowReaderExceptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(AsyncRowReaderExceptionTest, CancelMidStream) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        // "r1" will be returned to the caller.
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(MakeResponse({{"r1", true}, {"r2", false}}));
        });
        // At this point, the caller cancels the async streaming read loop by
        // either returning `false` in the on_row callback, or by setting an
        // exception on the promise (depending on the value of `GetParam()`).
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(EndOfStream());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r1", row.row_key());
    promise<bool> p;
    auto param = GetParam();
    if (param == "false-value") {
      p.set_value(false);
    }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    if (param == "std-exception") {
      try {
        throw std::runtime_error("user threw std::exception");
      } catch (...) {
        p.set_exception(std::current_exception());
      }
    } else if (param == "other-exception") {
      try {
        throw 5;
      } catch (...) {
        p.set_exception(std::current_exception());
      }
    }
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    return p.get_future();
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    std::string message;
    auto param = GetParam();
    if (param == "false-value") {
      message = "User cancelled";
    }
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    if (param == "std-exception") {
      message = "user threw std::exception";
    } else if (param == "other-exception") {
      message = "unknown exception";
    }
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    EXPECT_THAT(status, StatusIs(StatusCode::kCancelled, HasSubstr(message)));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
INSTANTIATE_TEST_SUITE_P(, AsyncRowReaderExceptionTest,
                         Values("false-value", "std-exception",
                                "other-exception"));
#else   // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
INSTANTIATE_TEST_SUITE_P(, AsyncRowReaderExceptionTest, Values("false-value"));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Like CancelMidStream but after the underlying stream has finished.
TEST(AsyncRowReaderTest, CancelAfterStreamFinish) {
  CompletionQueue cq;

  // First two rows are going to be processed, but third will cause the parser
  // to fail (row order violation). This will result in finishing the stream
  // while still keeping the two processed rows for the user.
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(
              MakeResponse({{"r1", true}, {"r2", true}, {"r0", true}}));
        });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(EndOfStream());
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r1", row.row_key());
    // Do not ask for any more rows.
    return make_ready_future(false);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

/// @test Verify that the recursion described in TryGiveRowToUser is bounded.
TEST(AsyncRowReaderTest, DeepStack) {
  // This test will return rows: "000", "001", ..., "100" in a single response.
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  // We can have many rows ready at once, and we return them recursively to the
  // caller. The stack can grow, so we impose a limit of 100 rows to return in
  // the same thread for this asynchronous call. When we hit the limit, we call
  // `CQ::RunAsync` to move the work onto a different thread.
  EXPECT_CALL(*mock_cq, RunAsync)
      .WillOnce([](std::unique_ptr<internal::RunAsyncBase> f) {
        // Do the work (which is to give row "099" to the caller).
        f->exec();
      })
      .WillOnce([](std::unique_ptr<internal::RunAsyncBase> f) {
        // Do the work (which is to give row "100" to the caller).
        f->exec();
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              int i = 0;
              std::vector<std::pair<std::string, bool>> v(101);
              std::generate_n(v.begin(), 101, [&i] {
                std::stringstream s_idx;
                s_idx << std::setfill('0') << std::setw(3) << i++;
                return std::make_pair(s_idx.str(), true);
              });
              return make_ready_future(MakeResponse(std::move(v)));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  int row_index = 0;
  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .Times(101)
      .WillRepeatedly([&row_index](bigtable::Row const& row) {
        EXPECT_EQ(row_index++, std::stoi(row.row_key()));
        return make_ready_future(true);
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

TEST(AsyncRowReaderTest, TimerErrorEndsLoop) {
  // Simulate a timer error (likely due to the CQ being shutdown). We should not
  // retry in this case.
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future<StatusOr<std::chrono::system_clock::time_point>>(
        Status(StatusCode::kCancelled, "timer cancelled"));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = absl::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{"r1", true}, {"r2", false}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).WillOnce([](bigtable::Row const& row) {
    EXPECT_EQ("r1", row.row_key());
    return make_ready_future(true);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), std::move(retry),
                         std::move(mock_b));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
