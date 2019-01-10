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
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
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
using namespace google::cloud::testing_util::chrono_literals;
using namespace ::testing;
using MockAsyncCheckAndMutateRowReader =
    google::cloud::bigtable::testing::MockAsyncResponseReader<
        google::bigtable::v2::CheckAndMutateRowResponse>;

class NoexTableAsyncCheckAndMutateRowTest
    : public bigtable::testing::internal::TableTestFixture {};

/// @test Verify that Table::CheckAndMutateRow() works in a simplest case.
TEST_F(NoexTableAsyncCheckAndMutateRowTest, Simple) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Simulate a transient failure first.
  auto reader =
      google::cloud::internal::make_unique<MockAsyncCheckAndMutateRowReader>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](btproto::CheckAndMutateRowResponse* response,
                          grpc::Status* status, void*) {
        response->set_predicate_matched(true);
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncCheckAndMutateRow(_, _, _))
      .WillOnce(Invoke([&reader](grpc::ClientContext*,
                                 btproto::CheckAndMutateRowRequest const&,
                                 grpc::CompletionQueue*) {
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckAndMutateRowResponse>>(reader.get());
      }));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  table_.AsyncCheckAndMutateRow(
      cq,
      [&op_called, &capture_status](CompletionQueue& cq, bool response,
                                    grpc::Status const& status) {
        EXPECT_TRUE(response);
        op_called = true;
        capture_status = status;
      },
      "foo", bt::Filter::PassAllFilter(),
      {bt::SetCell("fam", "col", 0_ms, "it was true")},
      {bt::SetCell("fam", "col", 0_ms, "it was false")});

  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_TRUE(capture_status.ok());
  EXPECT_EQ("mocked-status", capture_status.error_message());
}

/// @test Verify that Table::CheckAndMutateRow() works on failure.
TEST_F(NoexTableAsyncCheckAndMutateRowTest, Failure) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Simulate a transient failure first.
  auto reader =
      google::cloud::internal::make_unique<MockAsyncCheckAndMutateRowReader>();
  EXPECT_CALL(*reader, Finish(_, _, _))
      .WillOnce(Invoke([](btproto::CheckAndMutateRowResponse* response,
                          grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncCheckAndMutateRow(_, _, _))
      .WillOnce(Invoke([&reader](grpc::ClientContext*,
                                 btproto::CheckAndMutateRowRequest const&,
                                 grpc::CompletionQueue*) {
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckAndMutateRowResponse>>(reader.get());
      }));

  // Make the asynchronous request.
  bool op_called = false;
  grpc::Status capture_status;
  table_.AsyncCheckAndMutateRow(
      cq,
      [&op_called, &capture_status](CompletionQueue& cq, bool response,
                                    grpc::Status const& status) {
        op_called = true;
        capture_status = status;
      },
      "foo", bt::Filter::PassAllFilter(),
      {bt::SetCell("fam", "col", 0_ms, "it was true")},
      {bt::SetCell("fam", "col", 0_ms, "it was false")});

  EXPECT_FALSE(op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_FALSE(capture_status.ok());
  EXPECT_TRUE(capture_status.error_message().find("mocked-status") !=
              std::string::npos);
}

TEST_F(NoexTableAsyncCheckAndMutateRowTest, RetryFailure) {
  auto impl = std::make_shared<testing::MockCompletionQueue>();
  bigtable::CompletionQueue cq(impl);

  // Simulate a transient failure first.
  auto reader_1 =
      google::cloud::internal::make_unique<MockAsyncCheckAndMutateRowReader>();
  EXPECT_CALL(*reader_1, Finish(_, _, _))
      .WillOnce(Invoke([](btproto::CheckAndMutateRowResponse* response,
                          grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "mocked-status");
      }));
  auto reader_2 =
      google::cloud::internal::make_unique<MockAsyncCheckAndMutateRowReader>();
  EXPECT_CALL(*reader_2, Finish(_, _, _))
      .WillOnce(Invoke([](btproto::CheckAndMutateRowResponse* response,
                          grpc::Status* status, void*) {
        *status = grpc::Status(grpc::StatusCode::OK, "mocked-status");
      }));

  EXPECT_CALL(*client_, AsyncCheckAndMutateRow(_, _, _))
      .WillOnce(Invoke([&reader_1](grpc::ClientContext*,
                                   btproto::CheckAndMutateRowRequest const&,
                                   grpc::CompletionQueue*) {
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckAndMutateRowResponse>>(reader_1.get());
      }))
      .WillOnce(Invoke([&reader_2](grpc::ClientContext*,
                                   btproto::CheckAndMutateRowRequest const&,
                                   grpc::CompletionQueue*) {
        // This is safe, see comments in MockAsyncResponseReader.
        return std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
            btproto::CheckAndMutateRowResponse>>(reader_2.get());
      }));

  // Make the asynchronous request.
  bool user_op_called = false;
  grpc::Status capture_status;
  bigtable::noex::Table table(client_, kTableId, AlwaysRetryMutationPolicy());
  table.AsyncCheckAndMutateRow(
      cq,
      [&user_op_called, &capture_status](CompletionQueue& cq, bool response,
                                         grpc::Status const& status) {
        user_op_called = true;
        capture_status = status;
      },
      "foo", bt::Filter::PassAllFilter(),
      {bt::SetCell("fam", "col", 0_ms, "it was true")},
      {bt::SetCell("fam", "col", 0_ms, "it was false")});
  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  // After first failure, the timer is scheduled.
  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  // After the timer expires, a retry is submitted.
  EXPECT_FALSE(user_op_called);
  EXPECT_EQ(1U, impl->size());
  impl->SimulateCompletion(cq, true);

  EXPECT_TRUE(user_op_called);
  EXPECT_TRUE(impl->empty());

  EXPECT_TRUE(capture_status.ok());
  EXPECT_THAT(capture_status.error_message(), HasSubstr("mocked-status"));
}

}  // namespace
}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
