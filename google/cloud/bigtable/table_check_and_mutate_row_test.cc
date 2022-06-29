// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;  // NOLINT

class TableCheckAndMutateRowTest : public bigtable::testing::TableTestFixture {
 protected:
  TableCheckAndMutateRowTest() : TableTestFixture(CompletionQueue{}) {}

  template <typename Request>
  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        Request const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request, google::cloud::internal::ApiClientHeader());
  }

  std::function<
      grpc::Status(grpc::ClientContext* context,
                   google::bigtable::v2::CheckAndMutateRowRequest const&,
                   google::bigtable::v2::CheckAndMutateRowResponse*)>
  CreateCheckAndMutateMock(grpc::Status const& status) {
    return [this, status](
               grpc::ClientContext* context,
               google::bigtable::v2::CheckAndMutateRowRequest const& request,
               google::bigtable::v2::CheckAndMutateRowResponse*) {
      IsContextMDValid(
          *context, "google.bigtable.v2.Bigtable.CheckAndMutateRow", request);
      return status;
    };
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

/// @test Verify that Table::CheckAndMutateRow() works in a simplest case.
TEST_F(TableCheckAndMutateRowTest, Simple) {
  EXPECT_CALL(*client_, CheckAndMutateRow)
      .WillOnce(CreateCheckAndMutateMock(grpc::Status::OK));

  auto mut = table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")});

  ASSERT_STATUS_OK(mut);
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::CheckAndMutateRow() raises an on failures.
TEST_F(TableCheckAndMutateRowTest, Failure) {
  EXPECT_CALL(*client_, CheckAndMutateRow)
      .WillRepeatedly(CreateCheckAndMutateMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_FALSE(table_.CheckAndMutateRow(
      "foo", bigtable::Filter::PassAllFilter(),
      {bigtable::SetCell("fam", "col", 0_ms, "it was true")},
      {bigtable::SetCell("fam", "col", 0_ms, "it was false")}));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
