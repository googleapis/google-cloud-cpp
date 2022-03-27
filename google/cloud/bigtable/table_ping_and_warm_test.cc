// Copyright 2022 Google Inc.
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

#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;

class TablePingAndWarmTest : public bigtable::testing::TableTestFixture {
 protected:
  TablePingAndWarmTest() : TableTestFixture(CompletionQueue{}) {}

  Status IsContextMDValid(grpc::ClientContext& context,
                          std::string const& method) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, google::cloud::internal::ApiClientHeader());
  }

  std::function<grpc::Status(grpc::ClientContext* context,
                             google::bigtable::v2::PingAndWarmRequest const&,
                             google::bigtable::v2::PingAndWarmResponse*)>
  CreatePingAndWarmMock(grpc::Status const& status) {
    return
        [this, status](grpc::ClientContext* context,
                       google::bigtable::v2::PingAndWarmRequest const& request,
                       google::bigtable::v2::PingAndWarmResponse*) {
          EXPECT_EQ(request.name(), kInstanceName);
          EXPECT_STATUS_OK(IsContextMDValid(
              *context, "google.bigtable.v2.Bigtable.PingAndWarm"));
          return status;
        };
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(TablePingAndWarmTest, Success) {
  EXPECT_CALL(*client_, PingAndWarm)
      .WillOnce(CreatePingAndWarmMock(grpc::Status::OK));

  auto status = table_.PingAndWarm();
  ASSERT_STATUS_OK(status);
}

TEST_F(TablePingAndWarmTest, PermanentFailure) {
  EXPECT_CALL(*client_, PingAndWarm)
      .WillRepeatedly(CreatePingAndWarmMock(
          grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh")));

  auto status = table_.PingAndWarm();
  EXPECT_THAT(status, StatusIs(StatusCode::kFailedPrecondition));
}

TEST_F(TablePingAndWarmTest, RetryThenSuccess) {
  EXPECT_CALL(*client_, PingAndWarm)
      .WillOnce(CreatePingAndWarmMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(CreatePingAndWarmMock((grpc::Status::OK)));

  auto status = table_.PingAndWarm();
  ASSERT_STATUS_OK(status);
}

TEST_F(TablePingAndWarmTest, RetryPolicyExhausted) {
  auto constexpr kNumRetries = 2;

  EXPECT_CALL(*client_, PingAndWarm)
      .Times(kNumRetries + 1)
      .WillRepeatedly(CreatePingAndWarmMock(
          grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  auto table =
      Table(client_, kTableId, LimitedErrorCountRetryPolicy(kNumRetries));
  auto status = table.PingAndWarm();
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
