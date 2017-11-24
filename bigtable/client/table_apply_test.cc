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

#include "bigtable/client/data.h"
#include <google/bigtable/v2/bigtable_mock.grpc.pb.h>

#include <gmock/gmock.h>

/// Define helper types and functions for this test.
namespace {
class MockClient : public bigtable::ClientInterface {
 public:
  MockClient() : mock_stub(new google::bigtable::v2::MockBigtableStub) {}

  MOCK_METHOD1(Open,
               std::unique_ptr<bigtable::Table>(std::string const& table_id));
  MOCK_CONST_METHOD0(Stub, google::bigtable::v2::Bigtable::StubInterface&());

  std::unique_ptr<google::bigtable::v2::MockBigtableStub> mock_stub;
};
}  // anonymous namespace

/// @test Verify that Table::Apply() works in a simplest case.
TEST(TableApplyTest, Simple) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(
          Invoke([&client]() -> google::bigtable::v2::Bigtable::StubInterface& {
            return *client.mock_stub;
          }));
  EXPECT_CALL(*client.mock_stub, MutateRow(_, _, _))
      .WillOnce(Return(grpc::Status::OK));

  bigtable::Table table(&client, "foo_table");

  EXPECT_NO_THROW(table.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0, "val")})));
}

/// @test Verify that Table::Apply() raises an exception on permanent failures.
TEST(TableApplyTest, Failure) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(
          Invoke([&client]() -> google::bigtable::v2::Bigtable::StubInterface& {
            return *client.mock_stub;
          }));
  EXPECT_CALL(*client.mock_stub, MutateRow(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh")));

  bigtable::Table table(&client, "foo_table");

  EXPECT_THROW(table.Apply(bigtable::SingleRowMutation(
                   "bar", {bigtable::SetCell("fam", "col", 0, "val")})),
               std::exception);
}

/// @test Verify that Table::Apply() retries on partial failures.
TEST(TableApplyTest, Retry) {
  MockClient client;
  using namespace ::testing;
  EXPECT_CALL(client, Stub())
      .WillRepeatedly(
          Invoke([&client]() -> google::bigtable::v2::Bigtable::StubInterface& {
            return *client.mock_stub;
          }));
  EXPECT_CALL(*client.mock_stub, MutateRow(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Return(grpc::Status::OK));

  bigtable::Table table(&client, "foo_table");

  EXPECT_NO_THROW(table.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0, "val")})));
}
