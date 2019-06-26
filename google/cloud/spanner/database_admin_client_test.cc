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

#include "google/cloud/spanner/database_admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;

// gmock makes clang-tidy very angry, disable a few warnings that we have no
// control over.
// NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
class MockDatabaseAdminClientStub : public internal::DatabaseAdminStub {
 public:
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(
      CreateDatabase,
      StatusOr<google::longrunning::Operation>(
          grpc::ClientContext&,
          google::spanner::admin::database::v1::CreateDatabaseRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(
      DropDatabase,
      Status(grpc::ClientContext&,
             google::spanner::admin::database::v1::DropDatabaseRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(GetOperation,
               StatusOr<google::longrunning::Operation>(
                   grpc::ClientContext&,
                   google::longrunning::GetOperationRequest const&));
};

/// @test Verify that a permanent error in CreateDatabase is immediately
/// reported.
TEST(DatabaseAdminClientTest, HandleCreateDatabaseError) {
  auto mock = std::make_shared<MockDatabaseAdminClientStub>();

  EXPECT_CALL(*mock, CreateDatabase(_, _))
      .WillOnce(
          Invoke([](grpc::ClientContext&, google::spanner::admin::database::v1::
                                              CreateDatabaseRequest const&) {
            return StatusOr<google::longrunning::Operation>(
                Status(StatusCode::kPermissionDenied, "uh-oh"));
          }));

  DatabaseAdminClient client(mock);
  auto fut = client.CreateDatabase("test-project", "test-instance", "test-db");
  EXPECT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(0)));
  auto db = fut.get();
  EXPECT_EQ(StatusCode::kPermissionDenied, db.status().code());
}

/// @test Verify that as in the polling loop are reported.
TEST(DatabaseAdminClientTest, HandleGetOperationError) {
  auto mock = std::make_shared<MockDatabaseAdminClientStub>();

  EXPECT_CALL(*mock, CreateDatabase(_, _))
      .WillOnce(
          Invoke([](grpc::ClientContext&, google::spanner::admin::database::v1::
                                              CreateDatabaseRequest const&) {
            google::longrunning::Operation op;
            op.set_name("test-operation-name");
            op.set_done(false);
            return make_status_or(std::move(op));
          }));
  EXPECT_CALL(*mock, GetOperation(_, _))
      .WillOnce(
          Invoke([](grpc::ClientContext&,
                    google::longrunning::GetOperationRequest const& request) {
            EXPECT_EQ("test-operation-name", request.name());
            return StatusOr<google::longrunning::Operation>(
                Status(StatusCode::kAborted, "oh noes"));
          }));

  DatabaseAdminClient client(mock);
  auto db =
      client.CreateDatabase("test-project", "test-instance", "test-db").get();
  EXPECT_EQ(StatusCode::kAborted, db.status().code());
}

/// @test Verify that longrunning operation completing on error works.
TEST(DatabaseAdminClientTest, HandleOperationDoneWithFailure) {
  auto mock = std::make_shared<MockDatabaseAdminClientStub>();

  EXPECT_CALL(*mock, CreateDatabase(_, _))
      .WillOnce(Invoke([](grpc::ClientContext&,
                          google::spanner::admin::database::v1::
                              CreateDatabaseRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(true);
        op.mutable_error()->set_code(static_cast<int>(StatusCode::kAborted));
        op.mutable_error()->set_message("bad stuff happened");
        return make_status_or(std::move(op));
      }));

  DatabaseAdminClient client(mock);
  auto db =
      client.CreateDatabase("test-project", "test-instance", "test-db").get();
  EXPECT_EQ(StatusCode::kAborted, db.status().code());
  EXPECT_THAT(db.status().message(), HasSubstr("bad stuff happened"));
}

/// @test Verify handling of longrunning operation without result.
TEST(DatabaseAdminClientTest, HandleOperationDoneWithoutContents) {
  auto mock = std::make_shared<MockDatabaseAdminClientStub>();

  EXPECT_CALL(*mock, CreateDatabase(_, _))
      .WillOnce(
          Invoke([](grpc::ClientContext&, google::spanner::admin::database::v1::
                                              CreateDatabaseRequest const&) {
            google::longrunning::Operation op;
            op.set_name("test-operation-name");
            op.set_done(true);
            return make_status_or(std::move(op));
          }));

  DatabaseAdminClient client(mock);
  auto db =
      client.CreateDatabase("test-project", "test-instance", "test-db").get();
  EXPECT_EQ(StatusCode::kUnknown, db.status().code());
}

/// @test Verify handling of longrunning operation with invalid contents.
TEST(DatabaseAdminClientTest, HandleOperationDoneWithInvalidContents) {
  auto mock = std::make_shared<MockDatabaseAdminClientStub>();

  EXPECT_CALL(*mock, CreateDatabase(_, _))
      .WillOnce(
          Invoke([](grpc::ClientContext&, google::spanner::admin::database::v1::
                                              CreateDatabaseRequest const&) {
            google::longrunning::Operation op;
            op.set_name("test-operation-name");
            op.set_done(true);
            google::spanner::admin::database::v1::DropDatabaseRequest invalid;
            op.mutable_response()->PackFrom(invalid);
            return make_status_or(std::move(op));
          }));

  DatabaseAdminClient client(mock);
  auto db =
      client.CreateDatabase("test-project", "test-instance", "test-db").get();
  EXPECT_EQ(StatusCode::kInternal, db.status().code());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
