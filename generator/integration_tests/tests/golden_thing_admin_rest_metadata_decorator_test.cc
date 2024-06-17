// Copyright 2022 Google LLC
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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_rest_metadata_decorator.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::MockGoldenThingAdminRestStub;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

future<StatusOr<google::longrunning::Operation>> LongrunningTransientError() {
  return make_ready_future(
      StatusOr<google::longrunning::Operation>(TransientError()));
}

TEST(ThingAdminRestMetadataDecoratorTest, FormatServerTimeoutMilliseconds) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_THAT(context.GetHeader("x-server-timeout"),
                        Contains("3.141"));
            return TransientError();
          })
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_THAT(context.GetHeader("x-server-timeout"),
                        Contains("3600.000"));
            return TransientError();
          })
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_THAT(context.GetHeader("x-server-timeout"),
                        Contains("0.123"));
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  {
    rest_internal::RestContext context;
    google::test::admin::database::v1::GetDatabaseRequest request;
    auto status = stub.GetDatabase(
        context,
        Options{}.set<ServerTimeoutOption>(std::chrono::milliseconds(3141)),
        request);
    EXPECT_EQ(TransientError(), status.status());
  }
  {
    rest_internal::RestContext context;
    google::test::admin::database::v1::GetDatabaseRequest request;
    auto status = stub.GetDatabase(
        context,
        Options{}.set<ServerTimeoutOption>(std::chrono::milliseconds(3600000)),
        request);
    EXPECT_EQ(TransientError(), status.status());
  }
  {
    rest_internal::RestContext context;
    google::test::admin::database::v1::GetDatabaseRequest request;
    auto status = stub.GetDatabase(
        context,
        Options{}.set<ServerTimeoutOption>(std::chrono::milliseconds(123)),
        request);
    EXPECT_EQ(TransientError(), status.status());
  }
}

TEST(ThingAdminRestMetadataDecoratorTest, ExplicitApiClientHeader) {
  // We use knowledge of the implementation to assert that testing a single RPC
  // is sufficient.
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains("test-client-header"));
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock, "test-client-header");
  rest_internal::RestContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, ServiceApiVersionNotSpecified) {
  // We use knowledge of the implementation to assert that testing a single RPC
  // is sufficient.
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-version"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock, "test-client-header");
  rest_internal::RestContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, GetDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, ListDatabases) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::ListDatabasesRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"),
                        Contains("test-user-project"));
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListDatabases(
      context, Options{}.set<UserProjectOption>("test-user-project"), request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, AsyncCreateDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce(
          [](CompletionQueue&,
             std::unique_ptr<rest_internal::RestContext> context, auto,
             google::test::admin::database::v1::CreateDatabaseRequest const&) {
            EXPECT_THAT(
                context->GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context->GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-goog-quota-user"),
                        Contains("test-quota-user"));
            EXPECT_THAT(context->GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-goog-request-params"), IsEmpty());
            return LongrunningTransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  CompletionQueue cq;
  auto context = std::make_unique<rest_internal::RestContext>();
  google::test::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.AsyncCreateDatabase(
      cq, std::move(context),
      internal::MakeImmutableOptions(
          Options{}.set<QuotaUserOption>("test-quota-user")),
      request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST(ThingAdminRestMetadataDecoratorTest, CreateDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, CreateDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::CreateDatabaseRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.CreateDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, AsyncUpdateDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext> context, auto,
                   google::test::admin::database::v1::
                       UpdateDatabaseDdlRequest const&) {
        EXPECT_THAT(
            context->GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context->GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context->GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context->GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context->GetHeader("x-goog-request-params"), IsEmpty());
        return LongrunningTransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  CompletionQueue cq;
  auto context = std::make_unique<rest_internal::RestContext>();
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.AsyncUpdateDatabaseDdl(
      cq, std::move(context), internal::MakeImmutableOptions({}), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST(ThingAdminRestMetadataDecoratorTest, UpdateDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, UpdateDatabaseDdl)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::test::admin::database::v1::
                       UpdateDatabaseDdlRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.UpdateDatabaseDdl(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, DropDatabaseExplicitRoutingMatch) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::DropDatabaseRequest const&)
              -> google::cloud::Status {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(
                context.GetHeader("x-goog-request-params")[0],
                AllOf(
                    HasSubstr(std::string("project=projects%2Fmy_project")),
                    HasSubstr(std::string("instance=instances%2Fmy_instance")),
                    HasSubstr(
                        std::string("database=databases%2Fmy_database"))));
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.DropDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(ThingAdminRestMetadataDecoratorTest, DropDatabaseExplicitRoutingNoMatch) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::DropDatabaseRequest const&)
              -> google::cloud::Status {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database("no-match");
  auto status = stub.DropDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(ThingAdminRestMetadataDecoratorTest, GetDatabaseDdl) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::GetDatabaseDdlRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  request.set_database(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetDatabaseDdl(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, SetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::iam::v1::SetIamPolicyRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.SetIamPolicy(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, GetIamPolicy) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::iam::v1::GetIamPolicyRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.GetIamPolicy(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, TestIamPermissions) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::iam::v1::TestIamPermissionsRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      "projects/my_project/instances/my_instance/databases/my_database");
  auto status = stub.TestIamPermissions(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, AsyncCreateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce(
          [](CompletionQueue&,
             std::unique_ptr<rest_internal::RestContext> context, auto,
             google::test::admin::database::v1::CreateBackupRequest const&) {
            EXPECT_THAT(
                context->GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context->GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-goog-request-params"), IsEmpty());
            return LongrunningTransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  CompletionQueue cq;
  auto context = std::make_unique<rest_internal::RestContext>();
  google::test::admin::database::v1::CreateBackupRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.AsyncCreateBackup(
      cq, std::move(context), internal::MakeImmutableOptions({}), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST(ThingAdminRestMetadataDecoratorTest, CreateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, CreateBackup)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::CreateBackupRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::CreateBackupRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.CreateBackup(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, GetBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::test::admin::database::v1::GetBackupRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto status = stub.GetBackup(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, UpdateBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::UpdateBackupRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto status = stub.UpdateBackup(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, DeleteBackup) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::DeleteBackupRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::DeleteBackupRequest request;
  request.set_name(
      "projects/my_project/instances/my_instance/backups/my_backup");
  auto status = stub.DeleteBackup(context, Options{}, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(ThingAdminRestMetadataDecoratorTest, ListBackups) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListBackups)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::ListBackupsRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ListBackupsRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListBackups(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, AsyncRestoreDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncRestoreDatabase)
      .WillOnce(
          [](CompletionQueue&,
             std::unique_ptr<rest_internal::RestContext> context, auto,
             google::test::admin::database::v1::RestoreDatabaseRequest const&) {
            EXPECT_THAT(
                context->GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context->GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context->GetHeader("x-goog-request-params"), IsEmpty());
            return LongrunningTransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  CompletionQueue cq;
  auto context = std::make_unique<rest_internal::RestContext>();
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.AsyncRestoreDatabase(
      cq, std::move(context), internal::MakeImmutableOptions({}), request);
  EXPECT_EQ(TransientError(), status.get().status());
}

TEST(ThingAdminRestMetadataDecoratorTest, RestoreDatabase) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, RestoreDatabase)
      .WillOnce(
          [](rest_internal::RestContext& context, Options const&,
             google::test::admin::database::v1::RestoreDatabaseRequest const&) {
            EXPECT_THAT(
                context.GetHeader("x-goog-api-client"),
                Contains(google::cloud::internal::GeneratedLibClientHeader()));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.RestoreDatabase(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::test::admin::database::v1::
                       ListDatabaseOperationsRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListDatabaseOperations(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(ThingAdminRestMetadataDecoratorTest, ListBackupOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce([](rest_internal::RestContext& context, Options const&,
                   google::test::admin::database::v1::
                       ListBackupOperationsRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::GeneratedLibClientHeader()));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenThingAdminRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent("projects/my_project/instances/my_instance");
  auto status = stub.ListBackupOperations(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
