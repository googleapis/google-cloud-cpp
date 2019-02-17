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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"

namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;

/// Define helper types and functions for this test.
namespace {
class TableCheckAndMutateRowTest : public bigtable::testing::TableTestFixture {
};
}  // anonymous namespace

/// @test Verify that Table::CheckAndMutateRow() works in a simplest case.
TEST_F(TableCheckAndMutateRowTest, Simple) {
  using namespace ::testing;

  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _))
      .WillOnce(Return(grpc::Status::OK));

  auto mut = table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")});

  ASSERT_STATUS_OK(mut);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::CheckAndMutateRow() raises an on failures.
TEST_F(TableCheckAndMutateRowTest, Failure) {
  using namespace ::testing;

  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_FALSE(table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
