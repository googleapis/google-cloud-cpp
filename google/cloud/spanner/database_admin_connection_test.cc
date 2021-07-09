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

#include "google/cloud/spanner/database_admin_connection.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/testing/mock_database_admin_stub.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/kms_key_name.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::spanner_testing::MockDatabaseAdminStub;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AnyOf;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;
namespace gcsa = ::google::spanner::admin::database::v1;

std::shared_ptr<DatabaseAdminConnection> CreateTestingConnection(
    std::shared_ptr<spanner_internal::DatabaseAdminStub> mock) {
  LimitedErrorCountRetryPolicy retry(/*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  GenericPollingPolicy<LimitedErrorCountRetryPolicy> polling(retry, backoff);
  Options opts;
  opts.set<SpannerRetryPolicyOption>(retry.clone());
  opts.set<SpannerBackoffPolicyOption>(backoff.clone());
  opts.set<SpannerPollingPolicyOption>(polling.clone());
  return spanner_internal::MakeDatabaseAdminConnectionForTesting(
      std::move(mock), std::move(opts));
}

/// @test Verify that successful case works.
TEST(DatabaseAdminConnectionTest, CreateDatabaseSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const database_name =
      "projects/test-project/instances/test-instance/databases/test-database";

  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateDatabaseRequest const& request) {
        EXPECT_FALSE(request.has_encryption_config());
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&database_name](
                    CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::Database response;
        response.set_name(database_name);
        response.set_state(gcsa::Database::READY);
        op.mutable_response()->PackFrom(response);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  EXPECT_EQ(dbase.FullName(), database_name);
  auto fut = conn->CreateDatabase({dbase, {}, {}});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), database_name);
  EXPECT_EQ(response->state(), gcsa::Database::READY);
  EXPECT_FALSE(response->has_encryption_config());
}

/// @test Verify creating a database with an encryption key.
TEST(DatabaseAdminClientTest, CreateDatabaseWithEncryption) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const database_name =
      "projects/test-project/instances/test-instance/databases/test-database";

  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateDatabaseRequest const& request) {
        EXPECT_TRUE(request.has_encryption_config());
        if (request.has_encryption_config()) {
          EXPECT_EQ(request.encryption_config().kms_key_name(),
                    "projects/test-project/locations/some-location/keyRings/"
                    "a-key-ring/cryptoKeys/a-key-name");
        }
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&database_name](
                    CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::Database response;
        response.set_name(database_name);
        response.set_state(gcsa::Database::READY);
        response.mutable_encryption_config()->set_kms_key_name(
            "projects/test-project/locations/some-location/keyRings/"
            "a-key-ring/cryptoKeys/some-key-name");
        op.mutable_response()->PackFrom(response);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  EXPECT_EQ(dbase.FullName(), database_name);
  KmsKeyName encryption_key("test-project", "some-location", "a-key-ring",
                            "a-key-name");
  auto fut = conn->CreateDatabase(
      {dbase, {}, CustomerManagedEncryption(std::move(encryption_key))});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), database_name);
  EXPECT_EQ(response->state(), gcsa::Database::READY);
  EXPECT_TRUE(response->has_encryption_config());
  if (response->has_encryption_config()) {
    EXPECT_EQ(
        response->encryption_config().kms_key_name(),
        "projects/test-project/locations/some-location/keyRings/a-key-ring/"
        "cryptoKeys/some-key-name");
  }
}

/// @test Verify that a permanent error in CreateDatabase is immediately
/// reported.
TEST(DatabaseAdminConnectionTest, HandleCreateDatabaseError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateDatabaseRequest const&) {
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto fut = conn->CreateDatabase({dbase, {}, {}});
  auto response = fut.get();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, GetDatabase) {
  auto constexpr kResponseText = R"pb(
    name: "projects/project/instances/instance/databases/database"
    state: READY
    create_time { seconds: 1625696199 nanos: 123456789 }
    restore_info {
      source_type: BACKUP
      backup_info {
        backup: "projects/project/instances/instance/backups/backup"
        create_time { seconds: 1625696099 nanos: 987564321 }
        source_database: "projects/project/instances/instance/databases/database"
        version_time { seconds: 1625696099 nanos: 987564321 }
      }
    }
    encryption_config {
      kms_key_name: "projects/project/locations/location/keyRings/ring/cryptoKeys/key"
    }
    version_retention_period: "7d"
    earliest_version_time { seconds: 1625696199 nanos: 123456789 }
    default_leader: "us-east5"
  )pb";
  gcsa::Database expected_response;
  ASSERT_TRUE(TextFormat::ParseFromString(kResponseText, &expected_response));

  auto mock = std::make_shared<MockDatabaseAdminStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::GetDatabaseRequest const& request) {
            EXPECT_EQ(request.name(), expected_response.name());
            return expected_response;
          });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response =
      conn->GetDatabase({Database("project", "instance", "database")});
  ASSERT_STATUS_OK(response);
  EXPECT_THAT(*response, IsProtoEqual(expected_response));
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, GetDatabasePermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetDatabase(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, GetDatabaseTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetDatabase)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetDatabase(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, GetDatabaseDdlSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";

  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::GetDatabaseDdlRequest const& request) {
        EXPECT_EQ(expected_name, request.database());
        gcsa::GetDatabaseDdlResponse response;
        response.add_statements("CREATE DATABASE test-database");
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetDatabaseDdl(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->statements_size());
  ASSERT_EQ("CREATE DATABASE test-database", response->statements(0));
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, GetDatabaseDdlPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetDatabaseDdl(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, GetDatabaseDdlTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetDatabaseDdl)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetDatabaseDdl(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that successful case works.
TEST(DatabaseAdminConnectionTest, UpdateDatabaseSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::UpdateDatabaseDdlRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::UpdateDatabaseDdlMetadata metadata;
        metadata.set_database("test-database");
        op.mutable_metadata()->PackFrom(metadata);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto fut = conn->UpdateDatabase(
      {dbase, {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"}});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->database(), "test-database");
}

/// @test Verify that a permanent error in UpdateDatabase is immediately
/// reported.
TEST(DatabaseAdminConnectionTest, UpdateDatabaseErrorInPoll) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::UpdateDatabaseDdlRequest const&) {
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto fut = conn->UpdateDatabase(
      {dbase, {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"}});
  auto response = fut.get();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that errors in the polling loop are reported.
TEST(DatabaseAdminConnectionTest, CreateDatabaseErrorInPoll) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateDatabaseRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_done(true);
        op.mutable_error()->set_code(
            static_cast<int>(grpc::StatusCode::PERMISSION_DENIED));
        op.mutable_error()->set_message("uh-oh");
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto response = conn->CreateDatabase({dbase, {}, {}}).get();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that errors in the polling loop are reported.
TEST(DatabaseAdminConnectionTest, UpdateDatabaseGetOperationError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::UpdateDatabaseDdlRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_done(true);
        op.mutable_error()->set_code(
            static_cast<int>(grpc::StatusCode::PERMISSION_DENIED));
        op.mutable_error()->set_message("uh-oh");
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto response =
      conn->UpdateDatabase(
              {dbase, {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"}})
          .get();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that we can list databases in multiple pages.
TEST(DatabaseAdminConnectionTest, ListDatabases) {
  constexpr char const* kDatabaseText[5] = {
      R"pb(
        name: "projects/project/instances/instance/databases/db-1"
        state: READY
        create_time { seconds: 1625696199 nanos: 111111111 }
        restore_info {
          source_type: BACKUP
          backup_info {
            backup: "projects/project/instances/instance/backups/backup"
            create_time { seconds: 1625696099 nanos: 111111111 }
            source_database: "projects/project/instances/instance/databases/db"
            version_time { seconds: 1625696099 nanos: 111111111 }
          }
        }
        encryption_config {
          kms_key_name: "projects/project/locations/location/keyRings/ring/cryptoKeys/key"
        }
        version_retention_period: "1d"
        earliest_version_time { seconds: 1625696199 nanos: 111111111 }
        default_leader: "us-east1"
      )pb",
      R"pb(
        name: "projects/project/instances/instance/databases/db-2"
        state: READY
        create_time { seconds: 1625696199 nanos: 222222222 }
        restore_info {
          source_type: BACKUP
          backup_info {
            backup: "projects/project/instances/instance/backups/backup"
            create_time { seconds: 1625696099 nanos: 222222222 }
            source_database: "projects/project/instances/instance/databases/db"
            version_time { seconds: 1625696099 nanos: 222222222 }
          }
        }
        encryption_config {
          kms_key_name: "projects/project/locations/location/keyRings/ring/cryptoKeys/key"
        }
        version_retention_period: "2d"
        earliest_version_time { seconds: 1625696199 nanos: 222222222 }
        default_leader: "us-east2"
      )pb",
      R"pb(
        name: "projects/project/instances/instance/databases/db-3"
        state: READY
        create_time { seconds: 1625696199 nanos: 333333333 }
        restore_info {
          source_type: BACKUP
          backup_info {
            backup: "projects/project/instances/instance/backups/backup"
            create_time { seconds: 1625696099 nanos: 333333333 }
            source_database: "projects/project/instances/instance/databases/db"
            version_time { seconds: 1625696099 nanos: 333333333 }
          }
        }
        encryption_config {
          kms_key_name: "projects/project/locations/location/keyRings/ring/cryptoKeys/key"
        }
        version_retention_period: "3d"
        earliest_version_time { seconds: 1625696199 nanos: 333333333 }
        default_leader: "us-east3"
      )pb",
      R"pb(
        name: "projects/project/instances/instance/databases/db-4"
        state: READY
        create_time { seconds: 1625696199 nanos: 444444444 }
        restore_info {
          source_type: BACKUP
          backup_info {
            backup: "projects/project/instances/instance/backups/backup"
            create_time { seconds: 1625696099 nanos: 444444444 }
            source_database: "projects/project/instances/instance/databases/db"
            version_time { seconds: 1625696099 nanos: 444444444 }
          }
        }
        encryption_config {
          kms_key_name: "projects/project/locations/location/keyRings/ring/cryptoKeys/key"
        }
        version_retention_period: "4d"
        earliest_version_time { seconds: 1625696199 nanos: 444444444 }
        default_leader: "us-east4"
      )pb",
      R"pb(
        name: "projects/project/instances/instance/databases/db-5"
        state: READY
        create_time { seconds: 1625696199 nanos: 555555555 }
        restore_info {
          source_type: BACKUP
          backup_info {
            backup: "projects/project/instances/instance/backups/backup"
            create_time { seconds: 1625696099 nanos: 555555555 }
            source_database: "projects/project/instances/instance/databases/db"
            version_time { seconds: 1625696099 nanos: 555555555 }
          }
        }
        encryption_config {
          kms_key_name: "projects/project/locations/location/keyRings/ring/cryptoKeys/key"
        }
        version_retention_period: "5d"
        earliest_version_time { seconds: 1625696199 nanos: 555555555 }
        default_leader: "us-east5"
      )pb",
  };
  gcsa::Database expected_databases[5];
  ASSERT_TRUE(
      TextFormat::ParseFromString(kDatabaseText[0], &expected_databases[0]));
  ASSERT_TRUE(
      TextFormat::ParseFromString(kDatabaseText[1], &expected_databases[1]));
  ASSERT_TRUE(
      TextFormat::ParseFromString(kDatabaseText[2], &expected_databases[2]));
  ASSERT_TRUE(
      TextFormat::ParseFromString(kDatabaseText[3], &expected_databases[3]));
  ASSERT_TRUE(
      TextFormat::ParseFromString(kDatabaseText[4], &expected_databases[4]));

  Instance in("project", "instance");
  std::string const expected_parent = in.FullName();
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::ListDatabasesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_TRUE(request.page_token().empty());

            gcsa::ListDatabasesResponse page;
            page.set_next_page_token("page-1");
            *page.add_databases() = expected_databases[0];
            *page.add_databases() = expected_databases[1];
            return make_status_or(page);
          })
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::ListDatabasesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-1", request.page_token());

            gcsa::ListDatabasesResponse page;
            page.set_next_page_token("page-2");
            *page.add_databases() = expected_databases[2];
            *page.add_databases() = expected_databases[3];
            return make_status_or(page);
          })
      .WillOnce(
          [&](grpc::ClientContext&, gcsa::ListDatabasesRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-2", request.page_token());

            gcsa::ListDatabasesResponse page;
            page.clear_next_page_token();
            *page.add_databases() = expected_databases[4];
            return make_status_or(page);
          });

  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<gcsa::Database> actual_databases;
  for (auto const& database : conn->ListDatabases({in})) {
    ASSERT_STATUS_OK(database);
    actual_databases.push_back(*database);
  }
  EXPECT_THAT(actual_databases,
              ElementsAre(IsProtoEqual(expected_databases[0]),
                          IsProtoEqual(expected_databases[1]),
                          IsProtoEqual(expected_databases[2]),
                          IsProtoEqual(expected_databases[3]),
                          IsProtoEqual(expected_databases[4])));
}

TEST(DatabaseAdminConnectionTest, ListDatabasesPermanentFailure) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListDatabases({in});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DatabaseAdminConnectionTest, ListDatabasesTooManyFailures) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListDatabases)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListDatabases({in});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that successful case works.
TEST(DatabaseAdminConnectionTest, RestoreDatabaseSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const database_name =
      "projects/test-project/instances/test-instance/databases/test-database";

  EXPECT_CALL(*mock, AsyncRestoreDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::RestoreDatabaseRequest const& request) {
        EXPECT_EQ(request.database_id(), "test-database");
        EXPECT_FALSE(request.has_encryption_config());
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&database_name](
                    CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::Database response;
        response.set_name(database_name);
        response.set_state(gcsa::Database::READY);
        op.mutable_response()->PackFrom(response);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  EXPECT_EQ(dbase.FullName(), database_name);
  Backup backup(Instance("test-project", "test-instance"), "test-backup");
  auto fut = conn->RestoreDatabase({dbase, backup.FullName(), {}});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), database_name);
  EXPECT_EQ(response->state(), gcsa::Database::READY);
  EXPECT_FALSE(response->has_encryption_config());
}

/// @test Verify that using an encryption key works.
TEST(DatabaseAdminClientTest, RestoreDatabaseWithEncryption) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const database_name =
      "projects/test-project/instances/test-instance/databases/test-database";

  EXPECT_CALL(*mock, AsyncRestoreDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::RestoreDatabaseRequest const& request) {
        EXPECT_EQ(request.database_id(), "test-database");
        EXPECT_TRUE(request.has_encryption_config());
        if (request.has_encryption_config()) {
          EXPECT_EQ(request.encryption_config().encryption_type(),
                    gcsa::RestoreDatabaseEncryptionConfig::
                        CUSTOMER_MANAGED_ENCRYPTION);
          EXPECT_EQ(request.encryption_config().kms_key_name(),
                    "projects/test-project/locations/some-location/keyRings/"
                    "a-key-ring/cryptoKeys/restore-key-name");
        }
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&database_name](
                    CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::Database response;
        response.set_name(database_name);
        response.set_state(gcsa::Database::READY);
        response.mutable_encryption_config()->set_kms_key_name(
            "projects/test-project/locations/some-location/keyRings/"
            "a-key-ring/cryptoKeys/restore-key-name");
        op.mutable_response()->PackFrom(response);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Instance instance("test-project", "test-instance");
  Database dbase(instance, "test-database");
  Backup backup(instance, "test-backup");
  KmsKeyName encryption_key("test-project", "some-location", "a-key-ring",
                            "restore-key-name");
  auto fut = conn->RestoreDatabase(
      {dbase, backup.FullName(),
       CustomerManagedEncryption(std::move(encryption_key))});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), database_name);
  EXPECT_EQ(response->state(), gcsa::Database::READY);
  EXPECT_TRUE(response->has_encryption_config());
  if (response->has_encryption_config()) {
    EXPECT_EQ(
        response->encryption_config().kms_key_name(),
        "projects/test-project/locations/some-location/keyRings/a-key-ring/"
        "cryptoKeys/restore-key-name");
  }
}

/// @test Verify that a permanent error in RestoreDatabase is immediately
/// reported.
TEST(DatabaseAdminConnectionTest, HandleRestoreDatabaseError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncRestoreDatabase)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::RestoreDatabaseRequest const&) {
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  Backup backup(Instance("test-project", "test-instance"), "test-backup");
  auto fut = conn->RestoreDatabase({dbase, backup.FullName(), {}});
  auto response = fut.get();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, GetIamPolicySuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  std::string const expected_role = "roles/spanner.databaseReader";
  std::string const expected_member = "user:foobar@example.com";

  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name, &expected_role, &expected_member](
                    grpc::ClientContext&,
                    google::iam::v1::GetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        google::iam::v1::Policy response;
        auto& binding = *response.add_bindings();
        binding.set_role(expected_role);
        *binding.add_members() = expected_member;
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetIamPolicy(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->bindings().size());
  ASSERT_EQ(expected_role, response->bindings().Get(0).role());
  ASSERT_EQ(1, response->bindings().Get(0).members().size());
  ASSERT_EQ(expected_member, response->bindings().Get(0).members().Get(0));
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, GetIamPolicyPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetIamPolicy(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, GetIamPolicyTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetIamPolicy)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetIamPolicy(
      {Database("test-project", "test-instance", "test-database")});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, SetIamPolicySuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  auto constexpr kPolicyText = R"pb(
    etag: "request-etag"
    bindings {
      role: "roles/spanner.databaseReader"
      members: "user:test-user-1@example.com"
      members: "user:test-user-2@example.com"
    }
  )pb";
  google::iam::v1::Policy expected_policy;
  ASSERT_TRUE(TextFormat::ParseFromString(kPolicyText, &expected_policy));

  auto mock = std::make_shared<MockDatabaseAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([&expected_name](
                    grpc::ClientContext&,
                    google::iam::v1::SetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        return Status(StatusCode::kUnavailable, "try-again");
      })
      .WillOnce([&expected_name, &expected_policy](
                    grpc::ClientContext&,
                    google::iam::v1::SetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        EXPECT_THAT(request.policy(), IsProtoEqual(expected_policy));
        auto response = expected_policy;
        response.set_etag("response-etag");
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->SetIamPolicy(
      {Database("test-project", "test-instance", "test-database"),
       expected_policy});
  EXPECT_STATUS_OK(response);
  expected_policy.set_etag("response-etag");
  EXPECT_THAT(*response, IsProtoEqual(expected_policy));
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, SetIamPolicyPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->SetIamPolicy(
      {Database("test-project", "test-instance", "test-database"), {}});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that request without the Etag field should fail with the first
/// transient error.
TEST(DatabaseAdminConnectionTest, SetIamPolicyNonIdempotent) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::Policy policy;
  auto response = conn->SetIamPolicy(
      {Database("test-project", "test-instance", "test-database"), policy});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that request with the Etag field is retried for transient
/// errors.
TEST(DatabaseAdminConnectionTest, SetIamPolicyIdempotent) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, SetIamPolicy)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::Policy policy;
  policy.set_etag("test-etag-value");
  auto response = conn->SetIamPolicy(
      {Database("test-project", "test-instance", "test-database"), policy});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, TestIamPermissionsSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  std::string const expected_permission = "spanner.databases.read";

  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name, &expected_permission](
                    grpc::ClientContext&,
                    google::iam::v1::TestIamPermissionsRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        EXPECT_EQ(1, request.permissions_size());
        EXPECT_EQ(expected_permission, request.permissions(0));
        google::iam::v1::TestIamPermissionsResponse response;
        response.add_permissions(expected_permission);
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->TestIamPermissions(
      {Database("test-project", "test-instance", "test-database"),
       {expected_permission}});
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->permissions_size());
  ASSERT_EQ(expected_permission, response->permissions(0));
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, TestIamPermissionsPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->TestIamPermissions(
      {Database("test-project", "test-instance", "test-database"), {}});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, TestIamPermissionsTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, TestIamPermissions)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->TestIamPermissions(
      {Database("test-project", "test-instance", "test-database"), {}});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that successful case works.
TEST(DatabaseAdminConnectionTest, CreateBackupSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Database dbase("test-project", "test-instance", "test-database");
  auto now = absl::Now();
  auto expire_time = MakeTimestamp(now + absl::Hours(7)).value();
  auto version_time = MakeTimestamp(now - absl::Hours(7)).value();

  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce([&dbase, &expire_time, &version_time](
                    CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    gcsa::CreateBackupRequest const& request) {
        EXPECT_EQ(request.parent(), dbase.instance().FullName());
        EXPECT_EQ(request.backup_id(), "test-backup");
        auto const& backup = request.backup();
        EXPECT_EQ(backup.database(), dbase.FullName());
        EXPECT_EQ(MakeTimestamp(backup.expire_time()).value(), expire_time);
        EXPECT_EQ(MakeTimestamp(backup.version_time()).value(), version_time);
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&expire_time, &version_time](
                    CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::Backup response;
        response.set_name("test-backup");
        response.set_state(gcsa::Backup::READY);
        *response.mutable_expire_time() =
            expire_time.get<protobuf::Timestamp>().value();
        *response.mutable_version_time() =
            version_time.get<protobuf::Timestamp>().value();
        *response.mutable_create_time() = MakeTimestamp(absl::Now())
                                              .value()
                                              .get<protobuf::Timestamp>()
                                              .value();
        op.mutable_response()->PackFrom(response);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto fut = conn->CreateBackup(
      {dbase, "test-backup", {}, expire_time, version_time, {}});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), "test-backup");
  EXPECT_EQ(response->state(), gcsa::Backup::READY);
  EXPECT_EQ(MakeTimestamp(response->expire_time()).value(), expire_time);
  EXPECT_EQ(MakeTimestamp(response->version_time()).value(), version_time);
  EXPECT_GT(MakeTimestamp(response->create_time()).value(), version_time);
  EXPECT_FALSE(response->has_encryption_info());
}

/// @test Verify that using an encryption key works.
TEST(DatabaseAdminClientTest, CreateBackupWithEncryption) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Database dbase("test-project", "test-instance", "test-database");

  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce([&dbase](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                         gcsa::CreateBackupRequest const& request) {
        EXPECT_EQ(request.parent(), dbase.instance().FullName());
        EXPECT_EQ(request.backup_id(), "test-backup");
        EXPECT_EQ(request.backup().database(), dbase.FullName());
        EXPECT_TRUE(request.has_encryption_config());
        if (request.has_encryption_config()) {
          EXPECT_EQ(
              request.encryption_config().encryption_type(),
              gcsa::CreateBackupEncryptionConfig::GOOGLE_DEFAULT_ENCRYPTION);
          EXPECT_THAT(request.encryption_config().kms_key_name(), IsEmpty());
        }
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        gcsa::Backup response;
        response.set_name("test-backup");
        response.set_state(gcsa::Backup::READY);
        response.mutable_encryption_info()->set_encryption_type(
            gcsa::EncryptionInfo::GOOGLE_DEFAULT_ENCRYPTION);
        op.mutable_response()->PackFrom(response);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto fut = conn->CreateBackup(
      {dbase, "test-backup", {}, {}, absl::nullopt, GoogleEncryption()});
  auto response = fut.get();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), "test-backup");
  EXPECT_EQ(response->state(), gcsa::Backup::READY);
  EXPECT_TRUE(response->has_encryption_info());
  if (response->has_encryption_info()) {
    EXPECT_EQ(response->encryption_info().encryption_type(),
              gcsa::EncryptionInfo::GOOGLE_DEFAULT_ENCRYPTION);
    EXPECT_THAT(response->encryption_info().kms_key_version(), IsEmpty());
  }
}

/// @test Verify cancellation.
TEST(DatabaseAdminConnectionTest, CreateBackupCancel) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  promise<void> p;

  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateBackupRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .Times(AtMost(1))
      .WillRepeatedly(
          [](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
             google::longrunning::CancelOperationRequest const& request) {
            EXPECT_EQ("test-operation-name", request.name());
            return make_ready_future(Status());
          });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&p](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                     google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(false);
        p.set_value();  // enable `cancel()` call in the main thread.
        return make_ready_future(make_status_or(std::move(op)));
      })
      .WillRepeatedly([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                         google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(false);
        return make_ready_future(make_status_or(std::move(op)));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto fut =
      conn->CreateBackup({dbase, "test-backup", {}, {}, absl::nullopt, {}});
  p.get_future().get();  // await first poll before `cancel()`
  fut.cancel();
  auto backup = fut.get();
  EXPECT_THAT(backup, StatusIs(AnyOf(StatusCode::kCancelled,
                                     StatusCode::kDeadlineExceeded)));
}

/// @test Verify that a permanent error in CreateBackup is immediately
/// reported.
TEST(DatabaseAdminConnectionTest, HandleCreateBackupError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   gcsa::CreateBackupRequest const&) {
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  Database dbase("test-project", "test-instance", "test-database");
  auto fut =
      conn->CreateBackup({dbase, "test-backup", {}, {}, absl::nullopt, {}});
  auto backup = fut.get();
  EXPECT_THAT(backup, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, GetBackupSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";

  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::GetBackupRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        gcsa::Backup response;
        response.set_name(request.name());
        response.set_state(gcsa::Backup::READY);
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetBackup(
      {Backup(Instance("test-project", "test-instance"), "test-backup")
           .FullName()});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(expected_name, response->name());
  EXPECT_FALSE(response->has_encryption_info());
}

/// @test Verify that GetBackup can return encryption info and key version.
TEST(DatabaseAdminClientTest, GetBackupWithEncryption) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";

  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::GetBackupRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        gcsa::Backup response;
        response.set_name(request.name());
        response.set_state(gcsa::Backup::READY);
        response.mutable_encryption_info()->set_encryption_type(
            gcsa::EncryptionInfo::CUSTOMER_MANAGED_ENCRYPTION);
        response.mutable_encryption_info()->set_kms_key_version(
            "projects/test-project/locations/some-location/keyRings/a-key-ring/"
            "cryptoKeys/a-key-name/cryptoKeyVersions/1");
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetBackup(
      {Backup(Instance("test-project", "test-instance"), "test-backup")
           .FullName()});
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->name(), expected_name);
  EXPECT_EQ(response->state(), gcsa::Backup::READY);
  EXPECT_TRUE(response->has_encryption_info());
  if (response->has_encryption_info()) {
    EXPECT_EQ(response->encryption_info().encryption_type(),
              gcsa::EncryptionInfo::CUSTOMER_MANAGED_ENCRYPTION);
    EXPECT_EQ(
        response->encryption_info().kms_key_version(),
        "projects/test-project/locations/some-location/keyRings/a-key-ring/"
        "cryptoKeys/a-key-name/cryptoKeyVersions/1");
  }
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, GetBackupPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetBackup(
      {Backup(Instance("test-project", "test-instance"), "test-backup")
           .FullName()});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, GetBackupTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, GetBackup)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto response = conn->GetBackup(
      {Backup(Instance("test-project", "test-instance"), "test-backup")
           .FullName()});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, DeleteBackupSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";

  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::DeleteBackupRequest const& request) {
        EXPECT_EQ(expected_name, request.name());
        return google::cloud::Status();
      });

  auto conn = CreateTestingConnection(std::move(mock));
  auto status = conn->DeleteBackup({expected_name});
  EXPECT_STATUS_OK(status);
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, DeleteBackupPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto status = conn->DeleteBackup(
      {"projects/test-project/instances/test-instance/backups/test-backup"});
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, DeleteBackupTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, DeleteBackup)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto status = conn->DeleteBackup(
      {"projects/test-project/instances/test-instance/backups/test-backup"});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that we can list backups in multiple pages.
TEST(DatabaseAdminConnectionTest, ListBackups) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");
  std::string const expected_parent = in.FullName();

  EXPECT_CALL(*mock, ListBackups)
      .WillOnce([&expected_parent](grpc::ClientContext&,
                                   gcsa::ListBackupsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());

        gcsa::ListBackupsResponse page;
        page.set_next_page_token("page-1");
        page.add_backups()->set_name("backup-1");
        page.add_backups()->set_name("backup-2");
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](grpc::ClientContext&,
                                   gcsa::ListBackupsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-1", request.page_token());

        gcsa::ListBackupsResponse page;
        page.set_next_page_token("page-2");
        page.add_backups()->set_name("backup-3");
        page.add_backups()->set_name("backup-4");
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](grpc::ClientContext&,
                                   gcsa::ListBackupsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-2", request.page_token());

        gcsa::ListBackupsResponse page;
        page.clear_next_page_token();
        page.add_backups()->set_name("backup-5");
        return make_status_or(page);
      });

  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<std::string> actual_names;
  for (auto const& backup : conn->ListBackups({in, ""})) {
    ASSERT_STATUS_OK(backup);
    actual_names.push_back(backup->name());
  }
  EXPECT_THAT(actual_names, ElementsAre("backup-1", "backup-2", "backup-3",
                                        "backup-4", "backup-5"));
}

TEST(DatabaseAdminConnectionTest, ListBackupsPermanentFailure) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListBackups)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListBackups({in, ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DatabaseAdminConnectionTest, ListBackupsTooManyFailures) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListBackups)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListBackups({in, ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that the successful case works.
TEST(DatabaseAdminConnectionTest, UpdateBackupSuccess) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";

  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&expected_name](grpc::ClientContext&,
                                 gcsa::UpdateBackupRequest const& request) {
        EXPECT_EQ(expected_name, request.backup().name());
        gcsa::Backup response;
        response.set_name(request.backup().name());
        response.set_state(gcsa::Backup::READY);
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(
      Backup(Instance("test-project", "test-instance"), "test-backup")
          .FullName());
  auto response = conn->UpdateBackup({request});
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(gcsa::Backup::READY, response->state());
  EXPECT_EQ(expected_name, response->name());
}

/// @test Verify that permanent errors are reported immediately.
TEST(DatabaseAdminConnectionTest, UpdateBackupPermanentError) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  auto response = conn->UpdateBackup({request});
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

/// @test Verify that too many transients errors are reported correctly.
TEST(DatabaseAdminConnectionTest, UpdateBackupTooManyTransients) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();

  EXPECT_CALL(*mock, UpdateBackup)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  auto response = conn->UpdateBackup({request});
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that we can list backup operations in multiple pages.
TEST(DatabaseAdminConnectionTest, ListBackupOperations) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");
  std::string const expected_parent = in.FullName();

  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce(
          [&expected_parent](grpc::ClientContext&,
                             gcsa::ListBackupOperationsRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_TRUE(request.page_token().empty());

            gcsa::ListBackupOperationsResponse page;
            page.set_next_page_token("page-1");
            page.add_operations()->set_name("op-1");
            page.add_operations()->set_name("op-2");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](grpc::ClientContext&,
                             gcsa::ListBackupOperationsRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-1", request.page_token());

            gcsa::ListBackupOperationsResponse page;
            page.set_next_page_token("page-2");
            page.add_operations()->set_name("op-3");
            page.add_operations()->set_name("op-4");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](grpc::ClientContext&,
                             gcsa::ListBackupOperationsRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-2", request.page_token());

            gcsa::ListBackupOperationsResponse page;
            page.clear_next_page_token();
            page.add_operations()->set_name("op-5");
            return make_status_or(page);
          });

  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<std::string> actual_names;
  for (auto const& operation : conn->ListBackupOperations({in, ""})) {
    ASSERT_STATUS_OK(operation);
    actual_names.push_back(operation->name());
  }
  EXPECT_THAT(actual_names,
              ElementsAre("op-1", "op-2", "op-3", "op-4", "op-5"));
}

TEST(DatabaseAdminConnectionTest, ListBackupOperationsPermanentFailure) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListBackupOperations({in, ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DatabaseAdminConnectionTest, ListBackupOperationsTooManyFailures) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListBackupOperations)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListBackupOperations({in, ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kUnavailable));
}

/// @test Verify that we can list database operations in multiple pages.
TEST(DatabaseAdminConnectionTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");
  std::string const expected_parent = in.FullName();

  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce([&expected_parent](
                    grpc::ClientContext&,
                    gcsa::ListDatabaseOperationsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());

        gcsa::ListDatabaseOperationsResponse page;
        page.set_next_page_token("page-1");
        page.add_operations()->set_name("op-1");
        page.add_operations()->set_name("op-2");
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    grpc::ClientContext&,
                    gcsa::ListDatabaseOperationsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-1", request.page_token());

        gcsa::ListDatabaseOperationsResponse page;
        page.set_next_page_token("page-2");
        page.add_operations()->set_name("op-3");
        page.add_operations()->set_name("op-4");
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    grpc::ClientContext&,
                    gcsa::ListDatabaseOperationsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-2", request.page_token());

        gcsa::ListDatabaseOperationsResponse page;
        page.clear_next_page_token();
        page.add_operations()->set_name("op-5");
        return make_status_or(page);
      });

  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<std::string> actual_names;
  for (auto const& operation : conn->ListDatabaseOperations({in, ""})) {
    ASSERT_STATUS_OK(operation);
    actual_names.push_back(operation->name());
  }
  EXPECT_THAT(actual_names,
              ElementsAre("op-1", "op-2", "op-3", "op-4", "op-5"));
}

TEST(DatabaseAdminConnectionTest, ListDatabaseOperationsPermanentFailure) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListDatabaseOperations({in, ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

TEST(DatabaseAdminConnectionTest, ListDatabaseOperationsTooManyFailures) {
  auto mock = std::make_shared<MockDatabaseAdminStub>();
  Instance in("test-project", "test-instance");

  EXPECT_CALL(*mock, ListDatabaseOperations)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));

  auto conn = CreateTestingConnection(std::move(mock));
  auto range = conn->ListDatabaseOperations({in, ""});
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kUnavailable));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
