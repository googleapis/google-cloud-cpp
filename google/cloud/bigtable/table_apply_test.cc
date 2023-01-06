// Copyright 2017 Google Inc.
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

namespace bigtable = ::google::cloud::bigtable;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;  // NOLINT

class TableApplyTest : public bigtable::testing::TableTestFixture {
 protected:
  TableApplyTest() : TableTestFixture(CompletionQueue{}) {}

  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request, google::cloud::internal::ApiClientHeader());
  }

  std::function<grpc::Status(grpc::ClientContext* context,
                             google::bigtable::v2::MutateRowRequest const&,
                             google::bigtable::v2::MutateRowResponse*)>
  CreateMutateRowMock(grpc::Status const& status) {
    return [this, status](grpc::ClientContext* context,
                          google::bigtable::v2::MutateRowRequest const& request,
                          google::bigtable::v2::MutateRowResponse*) {
      IsContextMDValid(*context, "google.bigtable.v2.Bigtable.MutateRow",
                       request);
      return status;
    };
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

/// @test Verify that Table::Apply() works in a simplest case.
TEST_F(TableApplyTest, Simple) {
  EXPECT_CALL(*client_, MutateRow)
      .WillOnce(CreateMutateRowMock(grpc::Status::OK));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  ASSERT_STATUS_OK(status);
}

/// @test Verify that Table::Apply() raises an exception on permanent failures.
TEST_F(TableApplyTest, Failure) {
  EXPECT_CALL(*client_, MutateRow)
      .WillRepeatedly(CreateMutateRowMock(
          grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh")));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_THAT(status, StatusIs(StatusCode::kFailedPrecondition));
}

/// @test Verify that Table::Apply() retries on partial failures.
TEST_F(TableApplyTest, Retry) {
  EXPECT_CALL(*client_, MutateRow)
      .WillOnce(CreateMutateRowMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(CreateMutateRowMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(CreateMutateRowMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(CreateMutateRowMock((grpc::Status::OK)));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  ASSERT_STATUS_OK(status);
}

/// @test Verify that Table::Apply() retries only idempotent mutations.
TEST_F(TableApplyTest, RetryIdempotent) {
  EXPECT_CALL(*client_, MutateRow)
      .WillRepeatedly(CreateMutateRowMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "not-idempotent", {bigtable::SetCell("fam", "col", "val")}));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
