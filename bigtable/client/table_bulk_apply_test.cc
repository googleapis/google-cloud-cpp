// Copyright 2017 Google Inc.
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

#include "bigtable/client/chrono_literals.h"
#include "bigtable/client/data.h"

#include <absl/memory/memory.h>
#include <gmock/gmock.h>
#include <google/bigtable/v2/bigtable_mock.grpc.pb.h>

/// Define types and functions used in the tests.
// TODO(#67) - refactor Mock classes to a testing/ subdirectory.
namespace {
class MockClient : public bigtable::ClientInterface {
 public:
  MockClient() : mock_stub(new google::bigtable::v2::MockBigtableStub) {}

  MOCK_METHOD1(Open,
               std::unique_ptr<bigtable::Table>(std::string const &table_id));
  MOCK_CONST_METHOD0(Stub, google::bigtable::v2::Bigtable::StubInterface &());

  std::unique_ptr<google::bigtable::v2::MockBigtableStub> mock_stub;
};

class MockReader : public grpc::ClientReaderInterface<
                       ::google::bigtable::v2::MutateRowsResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t *));
  MOCK_METHOD1(Read, bool(::google::bigtable::v2::MutateRowsResponse *));
};
}  // anonymous namespace

/// @test Verify that Table::BulkApply() works in the easy case.
TEST(TableApplyMultipleTest, Simple) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(Invoke(
          [&client]() -> google::bigtable::v2::Bigtable::StubInterface & {
            return *client.mock_stub;
          }));
  namespace btproto = ::google::bigtable::v2;
  auto reader = absl::make_unique<MockReader>();
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        {
          auto &e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client.mock_stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&reader](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return reader.release();
          }));

  namespace bt = bigtable;
  bt::Table table(&client, "foo_table");

  EXPECT_NO_THROW(table.BulkApply(bt::BulkMutation(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}))));
}

/// @test Verify that Table::BulkApply() retries partial failures.
TEST(TableApplyMultipleTest, RetryPartialFailure) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(Invoke(
          [&client]() -> google::bigtable::v2::Bigtable::StubInterface & {
            return *client.mock_stub;
          }));
  namespace btproto = ::google::bigtable::v2;

  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        // ... simulate a partial (recoverable) failure ...
        auto &e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::UNAVAILABLE);
        auto &e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client.mock_stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r2.release();
          }));

  namespace bt = bigtable;
  bt::Table table(&client, "foo_table");

  EXPECT_NO_THROW(table.BulkApply(bt::BulkMutation(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}))));
}

/// @test Verify that Table::BulkApply() handles permanent failures.
TEST(TableBulkApply, PermanentFailure) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(Invoke(
          [&client]() -> google::bigtable::v2::Bigtable::StubInterface & {
            return *client.mock_stub;
          }));
  namespace btproto = ::google::bigtable::v2;

  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        {
          auto &e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::OUT_OF_RANGE);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client.mock_stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r1.release();
          }));

  namespace bt = bigtable;
  bt::Table table(&client, "foo_table");

  EXPECT_THROW(table.BulkApply(bt::BulkMutation(
                   bt::SingleRowMutation(
                       "foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
                   bt::SingleRowMutation(
                       "bar", {bigtable::SetCell("fam", "col", 0, "qux")}))),
               std::exception);
}

/// @test Verify that Table::BulkApply() handles a terminated stream.
TEST(TableBulkApply, CanceledStream) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(Invoke(
          [&client]() -> google::bigtable::v2::Bigtable::StubInterface & {
            return *client.mock_stub;
          }));
  namespace btproto = ::google::bigtable::v2;

  // Simulate a stream that returns one success and then terminates.  We expect
  // the BulkApply() operation to retry the request, because the mutation is in
  // an undetermined state.  Well, it should retry assuming it is idempotent,
  // which happens to be the case in this test.
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // Create a second stream returned by the mocks when the client retries.
  auto r2 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client.mock_stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r2.release();
          }));

  namespace bt = bigtable;
  bt::Table table(&client, "foo_table");

  EXPECT_NO_THROW(table.BulkApply(bt::BulkMutation(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}))));
}

/// @test Verify that Table::BulkApply() reports correctly on too many errors.
TEST(TableBulkApply, TooManyFailures) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(Invoke(
          [&client]() -> google::bigtable::v2::Bigtable::StubInterface & {
            return *client.mock_stub;
          }));
  namespace btproto = ::google::bigtable::v2;

  namespace bt = bigtable;
  using namespace bigtable::chrono_literals;
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  bt::Table table(
      &client, "foo_table",
      // Configure the Table to stop at 3 failures.
      bt::LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      bt::ExponentialBackoffPolicy(10_us, 40_us),
      // TODO(#66) - it is annoying to set a policy we do not care about.
      bt::SafeIdempotentMutationPolicy());

  // Setup the mocks to fail more than 3 times.
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));

  auto create_cancelled_stream = [&](grpc::ClientContext *,
                                     btproto::MutateRowsRequest const &) {
    auto stream = absl::make_unique<MockReader>();
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream.release();
  };

  EXPECT_CALL(*client.mock_stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r1.release();
          }))
      .WillOnce(Invoke(create_cancelled_stream))
      .WillOnce(Invoke(create_cancelled_stream));

  EXPECT_THROW(table.BulkApply(bt::BulkMutation(
                   bt::SingleRowMutation(
                       "foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
                   bt::SingleRowMutation(
                       "bar", {bigtable::SetCell("fam", "col", 0, "qux")}))),
               std::exception);
}

/// @test Verify that Table::BulkApply() retries only idempotent mutations.
TEST(TableBulkApply, RetryOnlyIdempotent) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(Invoke(
          [&client]() -> google::bigtable::v2::Bigtable::StubInterface & {
            return *client.mock_stub;
          }));
  namespace btproto = ::google::bigtable::v2;

  // We will send both idempotent and non-idempotent mutations.  We prepare the
  // mocks to return an empty stream in the first RPC request.  That will force
  // the client to only retry the idempotent mutations.
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse *r) {
        {
          auto &e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client.mock_stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext *, btproto::MutateRowsRequest const &) {
            return r2.release();
          }));

  namespace bt = bigtable;
  bt::Table table(&client, "foo_table");
  try {
    table.BulkApply(bt::BulkMutation(
        bt::SingleRowMutation("not-idempotent",
                              {bigtable::SetCell("fam", "col", "baz")}),
        bt::SingleRowMutation("is-idempotent",
                              {bigtable::SetCell("fam", "col", 0, "qux")})));
  } catch (bt::PermanentMutationFailure const &ex) {
    ASSERT_EQ(ex.failures().size(), 1L);
    EXPECT_EQ(ex.failures()[0].original_index(), 0);
    EXPECT_EQ(ex.failures()[0].mutation().row_key(), "not-idempotent");
  } catch (std::exception const &ex) {
    FAIL() << "unexpected std::exception raised: " << ex.what();
  } catch (...) {
    FAIL() << "unexpected exception of unknown type raised";
  }
}
