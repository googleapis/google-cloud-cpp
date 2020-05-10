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
#include "google/cloud/bigtable/testing/validate_metadata.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"

namespace bigtable = google::cloud::bigtable;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::testing::_;
using ::testing::Invoke;

/// Define helper types and functions for this test.
namespace {

class TableCheckAndMutateRowTest : public bigtable::testing::TableTestFixture {
};

auto mock_check_and_mutate = [](grpc::Status const& status) {
  return [status](grpc::ClientContext* context,
                  google::bigtable::v2::CheckAndMutateRowRequest const&,
                  google::bigtable::v2::CheckAndMutateRowResponse*) {
    EXPECT_STATUS_OK(google::cloud::bigtable::testing::IsContextMDValid(
        *context, "google.bigtable.v2.Bigtable.CheckAndMutateRow"));
    return status;
  };
};

}  // namespace

/// @test Verify that Table::CheckAndMutateRow() works in a simplest case.
TEST_F(TableCheckAndMutateRowTest, Simple) {
  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _))
      .WillOnce(Invoke(mock_check_and_mutate(grpc::Status::OK)));

  auto mut = table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")});

  ASSERT_STATUS_OK(mut);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::CheckAndMutateRow() raises an on failures.
TEST_F(TableCheckAndMutateRowTest, Failure) {
  EXPECT_CALL(*client_, CheckAndMutateRow(_, _, _))
      .WillRepeatedly(Invoke(mock_check_and_mutate(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again"))));

  EXPECT_FALSE(table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
