// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/database_admin_metadata.h"
#include "google/cloud/spanner/internal/api_client_header.h"
#include "google/cloud/spanner/testing/mock_database_admin_stub.h"
#include "google/cloud/spanner/testing/validate_metadata.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::Invoke;
namespace gcsa = google::spanner::admin::database::v1;

class DatabaseAdminMetadataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockDatabaseAdminStub>();
    DatabaseAdminMetadata stub(mock_);
    expected_api_client_header_ = ApiClientHeader();
  }

  void TearDown() override {}

  Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockDatabaseAdminStub> mock_;
  std::string expected_api_client_header_;
};

TEST_F(DatabaseAdminMetadataTest, CreateDatabase) {
  EXPECT_CALL(*mock_, CreateDatabase(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              gcsa::CreateDatabaseRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context,
            "google.spanner.admin.database.v1.DatabaseAdmin."
            "CreateDatabase",
            expected_api_client_header_));
        return TransientError();
      }));

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::CreateDatabaseRequest request;
  request.set_parent(
      "projects/test-project-id/"
      "instances/test-instance-id");
  auto status = stub.CreateDatabase(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(DatabaseAdminMetadataTest, UpdateDatabase) {
  EXPECT_CALL(*mock_, UpdateDatabase(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              gcsa::UpdateDatabaseDdlRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context,
            "google.spanner.admin.database.v1.DatabaseAdmin."
            "UpdateDatabaseDdl",
            expected_api_client_header_));
        return TransientError();
      }));

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  auto status = stub.UpdateDatabase(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(DatabaseAdminMetadataTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              gcsa::DropDatabaseRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context,
            "google.spanner.admin.database.v1.DatabaseAdmin."
            "DropDatabase",
            expected_api_client_header_));
        return TransientError();
      }));

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  gcsa::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project-id/"
      "instances/test-instance-id/"
      "databases/test-database");
  auto status = stub.DropDatabase(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST_F(DatabaseAdminMetadataTest, GetOperation) {
  EXPECT_CALL(*mock_, GetOperation(_, _))
      .WillOnce(Invoke([this](grpc::ClientContext& context,
                              google::longrunning::GetOperationRequest const&) {
        EXPECT_STATUS_OK(spanner_testing::IsContextMDValid(
            context, "google.longrunning.Operations.GetOperation",
            expected_api_client_header_));
        return TransientError();
      }));

  DatabaseAdminMetadata stub(mock_);
  grpc::ClientContext context;
  google::longrunning::GetOperationRequest request;
  request.set_name("operations/fake-operation-name");
  auto status = stub.GetOperation(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
