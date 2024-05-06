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
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/strings/str_format.h"
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
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Pair;
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

class AsyncRowReaderTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;
};

/// @test Verify that successfully reading rows works.
TEST_F(AsyncRowReaderTest, Success) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
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
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), true);
}

/// @test Verify that reading works when the futures are not immediately
/// satisfied.
TEST_F(AsyncRowReaderTest, SuccessDelayedFuture) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);

  // Satisfy the futures
  p1.set_value(true);
  p2.set_value(true);
  p3.set_value(true);
}

/// @test Verify that a single row can span multiple responses.
TEST_F(AsyncRowReaderTest, ResponseInMultipleChunks) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

/// @test Verify that parser fails if the stream finishes prematurely.
TEST_F(AsyncRowReaderTest, ParserEofFailsOnUnfinishedRow) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

/// @test Check that we ignore HandleEndOfStream errors if enough rows were read
TEST_F(AsyncRowReaderTest, ParserEofDoesntFailOnUnfinishedRowIfRowLimit) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), 1,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

/// @test Verify that permanent errors are not retried and properly passed.
TEST_F(AsyncRowReaderTest, PermanentFailure) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, RetryPolicyExhausted) {
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
      .WillRepeatedly([this](Unused, auto context, Unused,
                             v2::ReadRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
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
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, RetryInfoHeeded) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce(
          [this](Unused, auto context, Unused, v2::ReadRowsRequest const&) {
            metadata_fixture_.SetServerMetadata(*context, {});
            auto stream = std::make_unique<MockAsyncReadRowsStream>();
            EXPECT_CALL(*stream, Start)
                .WillOnce(Return(ByMove(make_ready_future(false))));
            EXPECT_CALL(*stream, Finish).WillOnce([] {
              auto status = internal::PermissionDeniedError("try again");
              internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
              return make_ready_future(status);
            });
            return stream;
          })
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const&) {
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start)
            .WillOnce(Return(ByMove(make_ready_future(true))));
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(ByMove(make_ready_future(EndOfStream()))));
        EXPECT_CALL(*stream, Finish)
            .WillOnce(Return(ByMove(make_ready_future(Status()))));
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion);

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), true);
}

TEST_F(AsyncRowReaderTest, RetryInfoIgnored) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce(
          [this](Unused, auto context, Unused, v2::ReadRowsRequest const&) {
            metadata_fixture_.SetServerMetadata(*context, {});
            auto stream = std::make_unique<MockAsyncReadRowsStream>();
            EXPECT_CALL(*stream, Start)
                .WillOnce(Return(ByMove(make_ready_future(false))));
            EXPECT_CALL(*stream, Finish).WillOnce([] {
              auto status = internal::PermissionDeniedError("try again");
              internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
              return make_ready_future(status);
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

/// @test Verify that retries do not ask for rows we have already read.
TEST_F(AsyncRowReaderTest, RetrySkipsReadRows) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([this](Unused, auto context, Unused,
                       v2::ReadRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // The initial row set contains two rows: "r1" and "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // Because we have already received "r1", we should not ask for it
        // again. The row set for this call should only contain: "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r2"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).WillOnce(Return(ms(0)));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, std::move(retry), std::move(mock_b), false);
}

/// @test Verify that we do not retry at all if the rowset will be empty.
TEST_F(AsyncRowReaderTest, NoRetryIfRowSetIsEmpty) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // The initial row set contains one row: "r1".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, std::move(retry), std::move(mock_b), false);
}

/// @test Verify that the last scanned row is respected.
TEST_F(AsyncRowReaderTest, LastScannedRowKeyIsRespected) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([this](Unused, auto context, Unused,
                       v2::ReadRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // The initial row set contains three rows: "r1", "r2", and "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2", "r3"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // Because the service has scanned up to "r2", we should not ask for
        // "r2" again. The row set for this call should only contain: "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r3"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).WillOnce(Return(ms(0)));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, std::move(retry), std::move(mock_b), false);
}

/// @test Verify proper handling of bogus responses from the service.
TEST_F(AsyncRowReaderTest, ParserFailsOnOutOfOrderRowKeys) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

/// @test Verify canceling the stream by satisfying the futures with false
class AsyncRowReaderExceptionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(AsyncRowReaderExceptionTest, CancelMidStream) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
    EXPECT_THAT(status.error_info().metadata(),
                Contains(Pair("gl-cpp.error.origin", "client")));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
INSTANTIATE_TEST_SUITE_P(, AsyncRowReaderExceptionTest,
                         Values("false-value", "std-exception",
                                "other-exception"));
#else   // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
INSTANTIATE_TEST_SUITE_P(, AsyncRowReaderExceptionTest, Values("false-value"));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Like CancelMidStream but after the underlying stream has finished.
TEST_F(AsyncRowReaderTest, CancelAfterStreamFinish) {
  CompletionQueue cq;

  // First two rows are going to be processed, but third will cause the parser
  // to fail (row order violation). This will result in finishing the stream
  // while still keeping the two processed rows for the user.
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

/// @test Verify that the recursion described in TryGiveRowToUser is bounded.
TEST_F(AsyncRowReaderTest, DeepStack) {
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
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              int i = 0;
              std::vector<std::pair<std::string, bool>> v(101);
              std::generate_n(v.begin(), 101, [&i] {
                return std::make_pair(absl::StrFormat("%03d", i++), true);
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
        EXPECT_EQ(absl::StrFormat("%03d", row_index++), row.row_key());
        return make_ready_future(true);
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, TimerErrorEndsLoop) {
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
      .WillOnce([this](Unused, auto context, Unused,
                       v2::ReadRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, CurrentOptionsContinuedOnRetries) {
  struct TestOption {
    using Type = int;
  };

  promise<StatusOr<std::chrono::system_clock::time_point>> timer_promise;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([&timer_promise] {
    return timer_promise.get_future();
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .Times(2)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::ReadRowsRequest const&) {
        EXPECT_EQ(5, internal::CurrentOptions().get<TestOption>());
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(1).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);

  internal::OptionsSpan span(
      Options{}
          .set<internal::GrpcSetupOption>(mock_setup.AsStdFunction())
          .set<TestOption>(5));
  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);

  // Simulate the timer being satisfied in a thread with different prevailing
  // options than the calling thread.
  internal::OptionsSpan clear(Options{});
  timer_promise.set_value(make_status_or(std::chrono::system_clock::now()));
}

TEST_F(AsyncRowReaderTest, ReverseScanSuccess) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{"r2", true}, {"r1", true}}));
            })
            .WillOnce([] { return make_ready_future(EndOfStream()); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r2", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), true,
                         std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, ReverseScanFailsOnIncreasingRowKeyOrder) {
  CompletionQueue cq;

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([] {
          // The rows should be returned out of order for a reverse scan.
          return make_ready_future(MakeResponse({{"r1", true}, {"r2", true}}));
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
    return make_ready_future(true);
  });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kInternal));
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), true,
                         std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, ReverseScanResumption) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([this](Unused, auto context, Unused,
                       v2::ReadRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_TRUE(request.reversed());
        // The initial row set contains three rows: "r1", "r2", and "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2", "r3"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            // The service will return "r3". But it will also tell us that "r2"
            // has been scanned, before failing with a transient error.
            .WillOnce([] {
              return make_ready_future(MakeResponse({{"r3", true}}));
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
      .WillOnce([](Unused, Unused, Unused, v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        // Because the service has scanned up to "r2", we should not ask for
        // "r2" again. The row set for this call should only contain: "r1".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1"));
        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
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
  EXPECT_CALL(on_row, Call)
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r3", row.row_key());
        return make_ready_future(true);
      })
      .WillOnce([](bigtable::Row const& row) {
        EXPECT_EQ("r1", row.row_key());
        return make_ready_future(true);
      });

  MockFunction<void(Status const&)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    ASSERT_STATUS_OK(status);
  });

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).WillOnce(Return(ms(0)));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  AsyncRowReader::Create(
      cq, mock, kAppProfile, kTableName, on_row.AsStdFunction(),
      on_finish.AsStdFunction(), bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      true, std::move(retry), std::move(mock_b), false);
}

TEST_F(AsyncRowReaderTest, BigtableCookie) {
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce(
          [this](Unused, auto context, Unused, v2::ReadRowsRequest const&) {
            // Return a bigtable cookie in the first request.
            metadata_fixture_.SetServerMetadata(
                *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
            auto stream = std::make_unique<MockAsyncReadRowsStream>();
            EXPECT_CALL(*stream, Start).WillOnce([] {
              return make_ready_future(false);
            });
            EXPECT_CALL(*stream, Finish).WillOnce([] {
              return make_ready_future(TransientError());
            });
            return stream;
          })
      .WillOnce(
          [this](Unused, auto context, Unused, v2::ReadRowsRequest const&) {
            // Verify that the next request includes the bigtable cookie from
            // above.
            auto headers = metadata_fixture_.GetMetadata(*context);
            EXPECT_THAT(headers,
                        Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
            auto stream = std::make_unique<MockAsyncReadRowsStream>();
            EXPECT_CALL(*stream, Start).WillOnce([] {
              return make_ready_future(false);
            });
            EXPECT_CALL(*stream, Finish).WillOnce([] {
              return make_ready_future(internal::PermissionDeniedError("fail"));
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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).WillOnce(Return(ms(0)));

  AsyncRowReader::Create(cq, mock, kAppProfile, kTableName,
                         on_row.AsStdFunction(), on_finish.AsStdFunction(),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::IsActive;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::SizeIs;
using ErrorStream = internal::AsyncStreamingReadRpcError<v2::ReadRowsResponse>;

TEST_F(AsyncRowReaderTest, TracedBackoff) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](auto&, auto context, auto, auto const&) {
        metadata_fixture_.SetServerMetadata(*context);
        return std::make_unique<ErrorStream>(TransientError());
      });

  promise<void> p;
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto on_row = [](bigtable::Row const&) { return make_ready_future(true); };
  auto on_finish = [&p](Status const&) { p.set_value(); };

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);

  internal::OptionsSpan o(EnableTracing(Options{}));
  AsyncRowReader::Create(background.cq(), mock, kAppProfile, kTableName,
                         std::move(on_row), std::move(on_finish),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);

  // Block until the async call has completed.
  p.get_future().get();

  EXPECT_THAT(span_catcher->GetSpans(),
              AllOf(SizeIs(kNumRetries), Each(SpanNamed("Async Backoff"))));
}

TEST_F(AsyncRowReaderTest, CallSpanActiveThroughout) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = internal::MakeSpan("span");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this, span](auto&, auto context, auto, auto const&) {
        metadata_fixture_.SetServerMetadata(*context);
        EXPECT_THAT(span, IsActive());
        return std::make_unique<ErrorStream>(TransientError());
      });

  promise<void> p;
  internal::AutomaticallyCreatedBackgroundThreads background;
  auto on_row = [](bigtable::Row const&) { return make_ready_future(true); };
  auto on_finish = [&p](Status const&) { p.set_value(); };
  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);

  internal::OTelScope scope(span);
  internal::OptionsSpan o(EnableTracing(Options{}));
  AsyncRowReader::Create(background.cq(), mock, kAppProfile, kTableName,
                         std::move(on_row), std::move(on_finish),
                         bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
                         bigtable::Filter::PassAllFilter(), false,
                         std::move(retry), std::move(mock_b), false);

  // Block until the async call has completed.
  p.get_future().get();
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
