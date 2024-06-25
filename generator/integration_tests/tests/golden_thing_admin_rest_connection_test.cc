// Copyright 2023 Google LLC
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

#include "generator/integration_tests/golden/v1/golden_thing_admin_rest_connection.h"
#include "generator/integration_tests/golden/v1/golden_thing_admin_connection.h"
#include "generator/integration_tests/golden/v1/golden_thing_admin_options.h"
#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_option_defaults.h"
#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_rest_connection_impl.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::GoldenThingAdminDefaultOptions;
using ::google::cloud::golden_v1_internal::GoldenThingAdminRestConnectionImpl;
using ::google::cloud::golden_v1_internal::GoldenThingAdminRestStub;
using ::google::cloud::golden_v1_internal::MockGoldenThingAdminRestStub;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Return;

std::shared_ptr<GoldenThingAdminConnection> CreateTestingConnection(
    std::shared_ptr<GoldenThingAdminRestStub> mock) {
  GoldenThingAdminLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  GenericPollingPolicy<GoldenThingAdminLimitedErrorCountRetryPolicy,
                       ExponentialBackoffPolicy>
      polling(retry, backoff);
  auto options = GoldenThingAdminDefaultOptions(
      Options{}
          .set<GoldenThingAdminRetryPolicyOption>(retry.clone())
          .set<GoldenThingAdminBackoffPolicyOption>(backoff.clone())
          .set<GoldenThingAdminPollingPolicyOption>(polling.clone()));
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<GoldenThingAdminRestConnectionImpl>(
      std::move(background), std::move(mock), std::move(options));
}

google::longrunning::Operation CreateStartingOperation() {
  google::longrunning::Operation op;
  op.set_name("test-operation-name");
  op.set_done(false);
  return op;
}

/// @test Verify that we can list databases in multiple pages.
TEST(GoldenThingAdminConnectionTest, ListDatabases) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_parent =
      "projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(
          [&expected_parent](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::ListDatabasesRequest const&
                  request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_TRUE(request.page_token().empty());

            ::google::test::admin::database::v1::ListDatabasesResponse page;
            page.set_next_page_token("page-1");
            ::google::test::admin::database::v1::Database database;
            page.add_databases()->set_name("db-1");
            page.add_databases()->set_name("db-2");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::ListDatabasesRequest const&
                  request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-1", request.page_token());

            ::google::test::admin::database::v1::ListDatabasesResponse page;
            page.set_next_page_token("page-2");
            ::google::test::admin::database::v1::Database database;
            page.add_databases()->set_name("db-3");
            page.add_databases()->set_name("db-4");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::ListDatabasesRequest const&
                  request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-2", request.page_token());

            ::google::test::admin::database::v1::ListDatabasesResponse page;
            page.clear_next_page_token();
            ::google::test::admin::database::v1::Database database;
            page.add_databases()->set_name("db-5");
            return make_status_or(page);
          });
  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<std::string> actual_names;
  ::google::test::admin::database::v1::ListDatabasesRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  for (auto const& database : conn->ListDatabases(request)) {
    ASSERT_STATUS_OK(database);
    actual_names.push_back(database->name());
  }
  EXPECT_THAT(actual_names,
              ElementsAre("db-1", "db-2", "db-3", "db-4", "db-5"));
}

TEST(GoldenThingAdminConnectionTest, ListDatabasesPermanentFailure) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListDatabasesRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListDatabases(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenThingAdminConnectionTest, ListDatabasesTooManyFailures) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListDatabasesRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListDatabases(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kDeadlineExceeded, begin->status().code());
}

/// @test Verify that successful case works.
TEST(GoldenThingAdminConnectionTest, CreateDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   ::google::test::admin::database::v1::
                       CreateDatabaseRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::Database database;
        database.set_name("test-database");
        op.mutable_response()->PackFrom(database);
        return make_ready_future(make_status_or(op));
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::CreateDatabaseRequest dbase;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->CreateDatabase(dbase);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto db = fut.get();
  ASSERT_STATUS_OK(db);
  EXPECT_EQ("test-database", db->name());
}

TEST(GoldenThingAdminConnectionTest, CreateDatabaseCancel) {
  auto const op = CreateStartingOperation();
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncCreateDatabase)
      .WillOnce([&](CompletionQueue&,
                    std::unique_ptr<rest_internal::RestContext>, auto,
                    ::google::test::admin::database::v1::
                        CreateDatabaseRequest const&) {
        return make_ready_future(make_status_or(op));
      });

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get;
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<rest_internal::RestContext>, auto,
                          google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return get.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::CancelOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return make_ready_future(Status{});
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::CreateDatabaseRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->CreateDatabase(request);
  get.PopFront().set_value(op);
  auto g = get.PopFront();
  fut.cancel();
  g.set_value(Status{StatusCode::kCancelled, "cancelled"});
  auto db = fut.get();
  EXPECT_THAT(db, StatusIs(StatusCode::kCancelled));
}

TEST(GoldenThingAdminConnectionTest, CreateDatabaseStartAwait) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  google::longrunning::Operation expected_operation;
  expected_operation.set_name("test-operation-name");
  google::test::admin::database::v1::CreateDatabaseMetadata metadata;
  expected_operation.mutable_metadata()->PackFrom(metadata);

  EXPECT_CALL(*mock, CreateDatabase(_, _, _)).WillOnce([&] {
    return make_status_or(expected_operation);
  });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([expected_operation](
                    CompletionQueue&, auto, auto,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ(expected_operation.name(), r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::Database database;
        database.set_name("test-database");
        op.mutable_response()->PackFrom(database);
        return make_ready_future(make_status_or(op));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::CreateDatabaseRequest dbase;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // TODO(#14344): Remove experimental tag.
  auto operation = conn->CreateDatabase(ExperimentalTag{}, NoAwaitTag{}, dbase);
  ASSERT_STATUS_OK(operation);
  EXPECT_THAT(operation->name(), Eq(expected_operation.name()));

  auto database = conn->CreateDatabase(ExperimentalTag{}, *operation).get();
  ASSERT_STATUS_OK(database);
  EXPECT_EQ("test-database", database->name());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, GetDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")))
      .WillOnce(
          [&expected_name](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::GetDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.name());
            ::google::test::admin::database::v1::Database response;
            response.set_name(request.name());
            response.set_state(
                ::google::test::admin::database::v1::Database::READY);
            return response;
          });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetDatabase(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Database::READY,
            response->state());
  EXPECT_EQ(expected_name, response->name());
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, GetDatabasePermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetDatabase(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that too many transients errors are reported correctly.
TEST(GoldenThingAdminConnectionTest, GetDatabaseTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseRequest request;
  request.set_name(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetDatabase(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that successful case works.
TEST(GoldenThingAdminConnectionTest, UpdateDatabaseDdlSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   ::google::test::admin::database::v1::
                       UpdateDatabaseDdlRequest const&) {
        ::google::test::admin::database::v1::UpdateDatabaseDdlMetadata metadata;
        metadata.set_database("test-database");
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::UpdateDatabaseDdlMetadata metadata;
        metadata.set_database("test-database");
        op.mutable_metadata()->PackFrom(metadata);
        return make_ready_future(make_status_or(op));
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  *request.add_statements() =
      "ALTER TABLE Albums ADD COLUMN MarketingBudget INT64";
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->UpdateDatabaseDdl(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto metadata = fut.get();
  ASSERT_STATUS_OK(metadata);
  EXPECT_EQ("test-database", metadata->database());
}

TEST(GoldenThingAdminConnectionTest, UpdateDatabaseDdlCancel) {
  auto const op = CreateStartingOperation();
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl)
      .WillOnce([&](CompletionQueue&,
                    std::unique_ptr<rest_internal::RestContext>, auto,
                    ::google::test::admin::database::v1::
                        UpdateDatabaseDdlRequest const&) {
        return make_ready_future(make_status_or(op));
      });

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get;
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<rest_internal::RestContext>, auto,
                          google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return get.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::CancelOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return make_ready_future(Status{});
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->UpdateDatabaseDdl(request);
  get.PopFront().set_value(op);
  auto g = get.PopFront();
  fut.cancel();
  g.set_value(Status{StatusCode::kCancelled, "cancelled"});
  auto db = fut.get();
  EXPECT_THAT(db, StatusIs(StatusCode::kCancelled));
}

TEST(GoldenThingAdminRestConnectionTest, UpdateDatabaseDdlStartAwait) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  google::longrunning::Operation expected_operation;
  expected_operation.set_name("test-operation-name");
  google::test::admin::database::v1::UpdateDatabaseDdlMetadata metadata;
  expected_operation.mutable_metadata()->PackFrom(metadata);

  EXPECT_CALL(*mock, UpdateDatabaseDdl(_, _, _)).WillOnce([&] {
    return make_status_or(expected_operation);
  });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([expected_operation](
                    CompletionQueue&, auto, auto,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ(expected_operation.name(), r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::UpdateDatabaseDdlMetadata metadata;
        metadata.set_database("test-database");
        op.mutable_metadata()->PackFrom(metadata);
        return make_ready_future(make_status_or(op));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  *request.add_statements() =
      "ALTER TABLE Albums ADD COLUMN MarketingBudget INT64";
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // TODO(#14344): Remove experimental tag.
  auto operation =
      conn->UpdateDatabaseDdl(ExperimentalTag{}, NoAwaitTag{}, request);
  ASSERT_STATUS_OK(operation);
  EXPECT_THAT(operation->name(), Eq(expected_operation.name()));

  auto update = conn->UpdateDatabaseDdl(ExperimentalTag{}, *operation).get();
  ASSERT_STATUS_OK(update);
  EXPECT_EQ("test-database", update->database());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, DropDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(
          [&expected_name](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::DropDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.database());
            return Status();
          });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->DropDatabase(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenThingAdminConnectionTest, DropDatabaseTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->DropDatabase(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.code());
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, DropDatabasePermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->DropDatabase(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.code());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, GetDatabaseDdlSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")))
      .WillOnce([&expected_name](rest_internal::RestContext&, Options const&,
                                 ::google::test::admin::database::v1::
                                     GetDatabaseDdlRequest const& request) {
        EXPECT_EQ(expected_name, request.database());
        ::google::test::admin::database::v1::GetDatabaseDdlResponse response;
        response.add_statements("CREATE DATABASE test-database");
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetDatabaseDdl(request);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->statements_size());
  ASSERT_EQ("CREATE DATABASE test-database", response->statements(0));
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, GetDatabaseDdlPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetDatabaseDdl(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that too many transients errors are reported correctly.
TEST(GoldenThingAdminConnectionTest, GetDatabaseDdlTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseDdlRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetDatabaseDdl(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, SetIamPolicySuccess) {
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  auto constexpr kText = R"pb(
    etag: "request-etag"
    bindings {
      role: "roles/spanner.databaseReader"
      members: "user:test-user-1@example.com"
      members: "user:test-user-2@example.com"
    }
  )pb";
  google::iam::v1::Policy expected_policy;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected_policy));
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([&expected_name, &expected_policy](
                    rest_internal::RestContext&, Options const&,
                    google::iam::v1::SetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        EXPECT_THAT(request.policy(), IsProtoEqual(expected_policy));
        auto response = expected_policy;
        response.set_etag("response-etag");
        return response;
      });

  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  *request.mutable_policy() = expected_policy;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->SetIamPolicy(request);
  EXPECT_STATUS_OK(response);
  expected_policy.set_etag("response-etag");
  EXPECT_THAT(*response, IsProtoEqual(expected_policy));
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, SetIamPolicyPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::Policy policy;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  *request.mutable_policy() = policy;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->SetIamPolicy(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that request without the Etag field should fail with the first
/// transient error.
TEST(GoldenThingAdminConnectionTest, SetIamPolicyNonIdempotent) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::Policy policy;
  google::iam::v1::SetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  *request.mutable_policy() = policy;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->SetIamPolicy(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, GetIamPolicySuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  std::string const expected_role = "roles/spanner.databaseReader";
  std::string const expected_member = "user:foobar@example.com";
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce([&expected_name, &expected_role, &expected_member](
                    rest_internal::RestContext&, Options const&,
                    google::iam::v1::GetIamPolicyRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        google::iam::v1::Policy response;
        auto& binding = *response.add_bindings();
        binding.set_role(expected_role);
        *binding.add_members() = expected_member;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetIamPolicy(request);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->bindings().size());
  ASSERT_EQ(expected_role, response->bindings().Get(0).role());
  ASSERT_EQ(1, response->bindings().Get(0).members().size());
  ASSERT_EQ(expected_member, response->bindings().Get(0).members().Get(0));
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, GetIamPolicyPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetIamPolicy(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that this http POST method is not retried.
TEST(GoldenThingAdminConnectionTest, GetIamPolicyTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::GetIamPolicyRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetIamPolicy(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, TestIamPermissionsSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  std::string const expected_permission = "spanner.databases.read";
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce([&expected_name, &expected_permission](
                    rest_internal::RestContext&, Options const&,
                    google::iam::v1::TestIamPermissionsRequest const& request) {
        EXPECT_EQ(expected_name, request.resource());
        EXPECT_EQ(1, request.permissions_size());
        EXPECT_EQ(expected_permission, request.permissions(0));
        google::iam::v1::TestIamPermissionsResponse response;
        response.add_permissions(expected_permission);
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  request.add_permissions(expected_permission);
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->TestIamPermissions(request);
  EXPECT_STATUS_OK(response);
  ASSERT_EQ(1, response->permissions_size());
  ASSERT_EQ(expected_permission, response->permissions(0));
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, TestIamPermissionsPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->TestIamPermissions(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that this http POST method is not retried.
TEST(GoldenThingAdminConnectionTest, TestIamPermissionsTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::iam::v1::TestIamPermissionsRequest request;
  request.set_resource(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->TestIamPermissions(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that successful case works.
TEST(GoldenThingAdminConnectionTest, CreateBackupSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce(
          [](CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
             auto,
             ::google::test::admin::database::v1::CreateBackupRequest const&) {
            google::longrunning::Operation op;
            op.set_name("test-operation-name");
            op.set_done(false);
            return make_ready_future(make_status_or(op));
          });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::Backup backup;
        backup.set_name("test-backup");
        op.mutable_response()->PackFrom(backup);
        return make_ready_future(make_status_or(op));
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::CreateBackupRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  request.set_backup_id("test-backup");
  request.mutable_backup()->set_name("test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->CreateBackup(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto backup = fut.get();
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ("test-backup", backup->name());
}

TEST(GoldenThingAdminConnectionTest, CreateBackupCancel) {
  auto const op = CreateStartingOperation();
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncCreateBackup)
      .WillOnce(
          [&](CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
              auto,
              ::google::test::admin::database::v1::CreateBackupRequest const&) {
            return make_ready_future(make_status_or(op));
          });

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get;
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<rest_internal::RestContext>, auto,
                          google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return get.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::CancelOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return make_ready_future(Status{});
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::CreateBackupRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->CreateBackup(request);
  get.PopFront().set_value(op);
  auto g = get.PopFront();
  fut.cancel();
  g.set_value(Status{StatusCode::kCancelled, "cancelled"});
  auto db = fut.get();
  EXPECT_THAT(db, StatusIs(StatusCode::kCancelled));
}

TEST(GoldenThingAdminConnectionTest, CreateBackupStartAwait) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  google::longrunning::Operation expected_operation;
  expected_operation.set_name("test-operation-name");
  google::test::admin::database::v1::CreateBackupMetadata metadata;
  expected_operation.mutable_metadata()->PackFrom(metadata);

  EXPECT_CALL(*mock, CreateBackup(_, _, _)).WillOnce([&] {
    return make_status_or(expected_operation);
  });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([expected_operation](
                    CompletionQueue&, auto, auto,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ(expected_operation.name(), r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::Backup backup;
        backup.set_name("test-backup");
        op.mutable_response()->PackFrom(backup);
        return make_ready_future(make_status_or(op));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::CreateBackupRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  request.set_backup_id("test-backup");
  request.mutable_backup()->set_name("test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // TODO(#14344): Remove experimental tag.
  auto operation = conn->CreateBackup(ExperimentalTag{}, NoAwaitTag{}, request);
  ASSERT_STATUS_OK(operation);
  EXPECT_THAT(operation->name(), Eq(expected_operation.name()));

  auto backup = conn->CreateBackup(ExperimentalTag{}, *operation).get();
  ASSERT_STATUS_OK(backup);
  EXPECT_EQ("test-backup", backup->name());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, GetBackupSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kDeadlineExceeded, "try-again")))
      .WillOnce([&expected_name](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::GetBackupRequest const&
                        request) {
        EXPECT_EQ(expected_name, request.name());
        ::google::test::admin::database::v1::Backup response;
        response.set_name(request.name());
        response.set_state(::google::test::admin::database::v1::Backup::READY);
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetBackupRequest request;
  request.set_name(expected_name);
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetBackup(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Backup::READY,
            response->state());
  EXPECT_EQ(expected_name, response->name());
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, GetBackupPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetBackupRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetBackup(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that too many transients errors are reported correctly.
TEST(GoldenThingAdminConnectionTest, GetBackupTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, GetBackup)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetBackupRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GetBackup(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, UpdateBackupSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";
  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(
          [&expected_name](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::UpdateBackupRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.backup().name());
            ::google::test::admin::database::v1::Backup response;
            response.set_name(request.backup().name());
            response.set_state(
                ::google::test::admin::database::v1::Backup::READY);
            return response;
          });
  auto conn = CreateTestingConnection(std::move(mock));
  google::test::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(
      "projects/test-project/instances/test-instance/backups/test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->UpdateBackup(request);
  EXPECT_STATUS_OK(response);
  EXPECT_EQ(::google::test::admin::database::v1::Backup::READY,
            response->state());
  EXPECT_EQ(expected_name, response->name());
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, UpdateBackupPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::test::admin::database::v1::UpdateBackupRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->UpdateBackup(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

/// @test Verify that http PATCH operation not retried.
TEST(GoldenThingAdminConnectionTest, UpdateBackupTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, UpdateBackup)
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  google::test::admin::database::v1::UpdateBackupRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->UpdateBackup(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, response.status().code());
}

/// @test Verify that the successful case works.
TEST(GoldenThingAdminConnectionTest, DeleteBackupSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/backups/test-backup";
  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(
          [&expected_name](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::DeleteBackupRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.name());
            return google::cloud::Status();
          });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DeleteBackupRequest request;
  request.set_name(expected_name);
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto status = conn->DeleteBackup(request);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that permanent errors are reported immediately.
TEST(GoldenThingAdminConnectionTest, DeleteBackupPermanentError) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DeleteBackupRequest request;
  request.set_name(
      "projects/test-project/instances/test-instance/backups/test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto status = conn->DeleteBackup(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, status.code());
}

/// @test Verify that http DELETE operation not retried.
TEST(GoldenThingAdminConnectionTest, DeleteBackupTooManyTransients) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, DeleteBackup)
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DeleteBackupRequest request;
  request.set_name(
      "projects/test-project/instances/test-instance/backups/test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto status = conn->DeleteBackup(request);
  EXPECT_EQ(StatusCode::kDeadlineExceeded, status.code());
}

/// @test Verify that we can list backups in multiple pages.
TEST(GoldenThingAdminConnectionTest, ListBackups) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_parent =
      "projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListBackups)
      .WillOnce(
          [&expected_parent](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::ListBackupsRequest const&
                  request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_TRUE(request.page_token().empty());
            ::google::test::admin::database::v1::ListBackupsResponse page;
            page.set_next_page_token("page-1");
            page.add_backups()->set_name("backup-1");
            page.add_backups()->set_name("backup-2");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::ListBackupsRequest const&
                  request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-1", request.page_token());
            ::google::test::admin::database::v1::ListBackupsResponse page;
            page.set_next_page_token("page-2");
            page.add_backups()->set_name("backup-3");
            page.add_backups()->set_name("backup-4");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](
              rest_internal::RestContext&, Options const&,
              ::google::test::admin::database::v1::ListBackupsRequest const&
                  request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-2", request.page_token());
            ::google::test::admin::database::v1::ListBackupsResponse page;
            page.clear_next_page_token();
            page.add_backups()->set_name("backup-5");
            return make_status_or(page);
          });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListBackupsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  std::vector<std::string> actual_names;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  for (auto const& backup : conn->ListBackups(request)) {
    ASSERT_STATUS_OK(backup);
    actual_names.push_back(backup->name());
  }
  EXPECT_THAT(actual_names, ElementsAre("backup-1", "backup-2", "backup-3",
                                        "backup-4", "backup-5"));
}

TEST(GoldenThingAdminConnectionTest, ListBackupsPermanentFailure) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListBackups)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListBackupsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListBackups(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenThingAdminConnectionTest, ListBackupsTooManyFailures) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListBackups)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListBackupsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListBackups(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kDeadlineExceeded, begin->status().code());
}

/// @test Verify that successful case works.
TEST(GoldenThingAdminConnectionTest, RestoreDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncRestoreDatabase)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   ::google::test::admin::database::v1::
                       RestoreDatabaseRequest const&) {
        google::longrunning::Operation op;
        op.set_name("test-operation-name");
        op.set_done(false);
        return make_ready_future(make_status_or(op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::Database database;
        database.set_name("test-database");
        op.mutable_response()->PackFrom(database);
        return make_ready_future(make_status_or(op));
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  request.set_database_id(
      "projects/test-project/instances/test-instance/databases/test-database");
  request.set_backup(
      "projects/test-project/instances/test-instance/backups/test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->RestoreDatabase(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto db = fut.get();
  ASSERT_STATUS_OK(db);
  EXPECT_EQ("test-database", db->name());
}

TEST(GoldenThingAdminConnectionTest, RestoreBackupCancel) {
  auto const op = CreateStartingOperation();
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncRestoreDatabase).WillOnce([&] {
    return make_ready_future(make_status_or(op));
  });

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get;
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<rest_internal::RestContext>, auto,
                          google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return get.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<rest_internal::RestContext>, auto,
                   google::longrunning::CancelOperationRequest const& r) {
        EXPECT_EQ("test-operation-name", r.name());
        return make_ready_future(Status{});
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::RestoreDatabaseRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->RestoreDatabase(request);
  get.PopFront().set_value(op);
  auto g = get.PopFront();
  fut.cancel();
  g.set_value(Status{StatusCode::kCancelled, "cancelled"});
  auto db = fut.get();
  EXPECT_THAT(db, StatusIs(StatusCode::kCancelled));
}

TEST(GoldenThingAdminConnectionTest, RestoreDatabaseStartAwait) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  google::longrunning::Operation expected_operation;
  expected_operation.set_name("test-operation-name");
  google::test::admin::database::v1::RestoreDatabaseMetadata metadata;
  expected_operation.mutable_metadata()->PackFrom(metadata);

  EXPECT_CALL(*mock, RestoreDatabase(_, _, _)).WillOnce([&] {
    return make_status_or(expected_operation);
  });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([expected_operation](
                    CompletionQueue&, auto, auto,
                    google::longrunning::GetOperationRequest const& r) {
        EXPECT_EQ(expected_operation.name(), r.name());
        google::longrunning::Operation op;
        op.set_name(r.name());
        op.set_done(true);
        ::google::test::admin::database::v1::Database database;
        database.set_name("test-database");
        op.mutable_response()->PackFrom(database);
        return make_ready_future(make_status_or(op));
      });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  request.set_database_id(
      "projects/test-project/instances/test-instance/databases/test-database");
  request.set_backup(
      "projects/test-project/instances/test-instance/backups/test-backup");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // TODO(#14344): Remove experimental tag.
  auto operation =
      conn->RestoreDatabase(ExperimentalTag{}, NoAwaitTag{}, request);
  ASSERT_STATUS_OK(operation);
  EXPECT_THAT(operation->name(), Eq(expected_operation.name()));

  auto database = conn->RestoreDatabase(ExperimentalTag{}, *operation).get();
  ASSERT_STATUS_OK(database);
  EXPECT_EQ("test-database", database->name());
}

/// @test Verify that we can list database operations in multiple pages.
TEST(GoldenThingAdminConnectionTest, ListDatabaseOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_parent =
      "projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(
          [&expected_parent](rest_internal::RestContext&, Options const&,
                             ::google::test::admin::database::v1::
                                 ListDatabaseOperationsRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_TRUE(request.page_token().empty());

            ::google::test::admin::database::v1::ListDatabaseOperationsResponse
                page;
            page.set_next_page_token("page-1");
            page.add_operations()->set_name("op-1");
            page.add_operations()->set_name("op-2");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](rest_internal::RestContext&, Options const&,
                             ::google::test::admin::database::v1::
                                 ListDatabaseOperationsRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-1", request.page_token());

            ::google::test::admin::database::v1::ListDatabaseOperationsResponse
                page;
            page.set_next_page_token("page-2");
            page.add_operations()->set_name("op-3");
            page.add_operations()->set_name("op-4");
            return make_status_or(page);
          })
      .WillOnce(
          [&expected_parent](rest_internal::RestContext&, Options const&,
                             ::google::test::admin::database::v1::
                                 ListDatabaseOperationsRequest const& request) {
            EXPECT_EQ(expected_parent, request.parent());
            EXPECT_EQ("page-2", request.page_token());

            ::google::test::admin::database::v1::ListDatabaseOperationsResponse
                page;
            page.clear_next_page_token();
            page.add_operations()->set_name("op-5");
            return make_status_or(page);
          });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  std::vector<std::string> actual_names;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  for (auto const& operation : conn->ListDatabaseOperations(request)) {
    ASSERT_STATUS_OK(operation);
    actual_names.push_back(operation->name());
  }
  EXPECT_THAT(actual_names,
              ElementsAre("op-1", "op-2", "op-3", "op-4", "op-5"));
}

TEST(GoldenThingAdminConnectionTest, ListDatabaseOperationsPermanentFailure) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListDatabaseOperations(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenThingAdminConnectionTest, ListDatabaseOperationsTooManyFailures) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListDatabaseOperations(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kDeadlineExceeded, begin->status().code());
}

/// @test Verify that we can list backup operations in multiple pages.
TEST(GoldenThingAdminConnectionTest, ListBackupOperations) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_parent =
      "projects/test-project/instances/test-instance";
  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce([&expected_parent](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::
                        ListBackupOperationsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());

        ::google::test::admin::database::v1::ListBackupOperationsResponse page;
        page.set_next_page_token("page-1");
        page.add_operations()->set_name("op-1");
        page.add_operations()->set_name("op-2");
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::
                        ListBackupOperationsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-1", request.page_token());
        ::google::test::admin::database::v1::ListBackupOperationsResponse page;
        page.set_next_page_token("page-2");
        page.add_operations()->set_name("op-3");
        page.add_operations()->set_name("op-4");
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::
                        ListBackupOperationsRequest const& request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-2", request.page_token());
        ::google::test::admin::database::v1::ListBackupOperationsResponse page;
        page.clear_next_page_token();
        page.add_operations()->set_name("op-5");
        return make_status_or(page);
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  std::vector<std::string> actual_names;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  for (auto const& operation : conn->ListBackupOperations(request)) {
    ASSERT_STATUS_OK(operation);
    actual_names.push_back(operation->name());
  }
  EXPECT_THAT(actual_names,
              ElementsAre("op-1", "op-2", "op-3", "op-4", "op-5"));
}

TEST(GoldenThingAdminConnectionTest, ListBackupOperationsPermanentFailure) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListBackupOperations(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenThingAdminConnectionTest, ListBackupOperationsTooManyFailures) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, ListBackupOperations)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kDeadlineExceeded, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent("projects/test-project/instances/test-instance");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListBackupOperations(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kDeadlineExceeded, begin->status().code());
}

TEST(GoldenThingAdminConnectionTest, AsyncGetDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncGetDatabase)
      .WillOnce(
          [](CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
             auto,
             ::google::test::admin::database::v1::GetDatabaseRequest const&) {
            google::test::admin::database::v1::Database db;
            db.set_name("test-database");
            return make_ready_future(make_status_or(db));
          });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseRequest dbase;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->AsyncGetDatabase(dbase);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto db = fut.get();
  ASSERT_STATUS_OK(db);
  EXPECT_EQ("test-database", db->name());
}

TEST(GoldenThingAdminConnectionTest, AsyncGetDatabaseTooManyFailures) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncGetDatabase)
      .Times(AtLeast(2))
      .WillRepeatedly(
          [](CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
             auto,
             ::google::test::admin::database::v1::GetDatabaseRequest const&) {
            return make_ready_future<
                StatusOr<google::test::admin::database::v1::Database>>(
                Status(StatusCode::kDeadlineExceeded, "try again"));
          });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseRequest dbase;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->AsyncGetDatabase(dbase);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto db = fut.get();
  ASSERT_THAT(db,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try again")));
  auto const& metadata = db.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", _)));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
}

TEST(GoldenThingAdminConnectionTest, AsyncGetDatabaseCancel) {
  promise<bool> cancelled;
  promise<StatusOr<google::test::admin::database::v1::Database>> p(
      [&cancelled] { cancelled.set_value(true); });
  auto cancel_completed = cancelled.get_future().then([&p](future<bool> f) {
    p.set_value(Status(StatusCode::kDeadlineExceeded, "try again"));
    return f.get();
  });

  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  EXPECT_CALL(*mock, AsyncGetDatabase)
      .WillOnce(
          [&p](CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
               auto,
               ::google::test::admin::database::v1::GetDatabaseRequest const&) {
            return p.get_future();
          });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GetDatabaseRequest dbase;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->AsyncGetDatabase(dbase);
  ASSERT_EQ(std::future_status::timeout,
            fut.wait_for(std::chrono::milliseconds(10)));
  EXPECT_FALSE(cancel_completed.is_ready());
  fut.cancel();
  EXPECT_TRUE(cancel_completed.get());
  auto db = fut.get();
  ASSERT_THAT(db,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try again")));
  auto const& metadata = db.status().error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", _)));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
}

TEST(GoldenThingAdminConnectionTest, AsyncDropDatabaseSuccess) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  EXPECT_CALL(*mock, AsyncDropDatabase)
      .WillOnce(
          [&expected_name](
              CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
              auto,
              ::google::test::admin::database::v1::DropDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.database());
            return make_ready_future(Status());
          });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->AsyncDropDatabase(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto status = fut.get();
  ASSERT_STATUS_OK(status);
}

TEST(GoldenThingAdminConnectionTest, AsyncDropDatabaseFailure) {
  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  EXPECT_CALL(*mock, AsyncDropDatabase)
      .Times(AtLeast(2))
      .WillRepeatedly(
          [&expected_name](
              CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
              auto,
              ::google::test::admin::database::v1::DropDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.database());
            return make_ready_future(
                Status(StatusCode::kDeadlineExceeded, "try again"));
          });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->AsyncDropDatabase(request);
  ASSERT_EQ(std::future_status::ready, fut.wait_for(std::chrono::seconds(10)));
  auto status = fut.get();
  ASSERT_THAT(status,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try again")));
  auto const& metadata = status.error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", _)));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason",
                                      "retry-policy-exhausted")));
}

TEST(GoldenThingAdminConnectionTest, AsyncDropDatabaseCancel) {
  promise<bool> cancelled;
  promise<Status> p([&cancelled] { cancelled.set_value(true); });
  auto cancel_completed = cancelled.get_future().then([&p](future<bool> f) {
    p.set_value(Status(StatusCode::kDeadlineExceeded, "try again"));
    return f.get();
  });

  auto mock = std::make_shared<MockGoldenThingAdminRestStub>();
  std::string const expected_name =
      "projects/test-project/instances/test-instance/databases/test-database";
  EXPECT_CALL(*mock, AsyncDropDatabase)
      .WillOnce(
          [&p, &expected_name](
              CompletionQueue&, std::unique_ptr<rest_internal::RestContext>,
              auto,
              ::google::test::admin::database::v1::DropDatabaseRequest const&
                  request) {
            EXPECT_EQ(expected_name, request.database());
            return p.get_future();
          });

  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::DropDatabaseRequest request;
  request.set_database(
      "projects/test-project/instances/test-instance/databases/test-database");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto fut = conn->AsyncDropDatabase(request);
  ASSERT_EQ(std::future_status::timeout,
            fut.wait_for(std::chrono::milliseconds(10)));
  EXPECT_FALSE(cancel_completed.is_ready());
  fut.cancel();
  EXPECT_TRUE(cancel_completed.get());
  auto status = fut.get();
  ASSERT_THAT(status,
              StatusIs(StatusCode::kDeadlineExceeded, HasSubstr("try again")));
  auto const& metadata = status.error_info().metadata();
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.function", _)));
  EXPECT_THAT(metadata, Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
}

TEST(GoldenThingAdminConnectionTest, CheckExpectedOptions) {
  struct UnexpectedOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  auto opts = Options{}.set<UnexpectedOption>({});
  auto conn = MakeGoldenThingAdminConnectionRest(std::move(opts));
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("Unexpected option.+UnexpectedOption")));
}

TEST(GoldenThingAdminConnectionTest, ConnectionCreatedWithOption) {
  auto opts = Options{}.set<EndpointOption>("foo");
  auto conn = MakeGoldenThingAdminConnectionRest(std::move(opts));
  ASSERT_TRUE(conn->options().has<EndpointOption>());
  EXPECT_THAT(conn->options().get<EndpointOption>(), Eq("foo"));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::Not;

TEST(GoldenThingAdminConnectionTest, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto options = EnableTracing(
      Options{}
          .set<EndpointOption>("localhost:1")
          .set<GoldenThingAdminRetryPolicyOption>(
              GoldenThingAdminLimitedErrorCountRetryPolicy(0).clone()));
  auto conn = MakeGoldenThingAdminConnectionRest(std::move(options));
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // Make a call, which should fail fast. The error itself is not important.
  (void)conn->DeleteBackup({});

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(SpanNamed(
                  "golden_v1::GoldenThingAdminConnection::DeleteBackup")));
}

TEST(GoldenThingAdminConnectionTest, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto options = DisableTracing(
      Options{}
          .set<EndpointOption>("localhost:1")
          .set<GoldenThingAdminRetryPolicyOption>(
              GoldenThingAdminLimitedErrorCountRetryPolicy(0).clone()));
  auto conn = MakeGoldenThingAdminConnectionRest(std::move(options));
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // Make a call, which should fail fast. The error itself is not important.
  (void)conn->DeleteBackup({});

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Not(Contains(SpanNamed(
                  "golden_v1::GoldenThingAdminConnection::DeleteBackup"))));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
