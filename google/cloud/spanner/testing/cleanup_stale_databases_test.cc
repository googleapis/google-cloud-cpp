// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/testing/cleanup_stale_databases.h"
#include "google/cloud/spanner/admin/mocks/mock_database_admin_connection.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace gsad = ::google::spanner::admin::database;
namespace spanner = ::google::cloud::spanner;

using ::google::cloud::spanner_admin_mocks::MockDatabaseAdminConnection;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::UnorderedElementsAreArray;

TEST(CleanupStaleDatabases, Empty) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  spanner::Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce([&expected_instance](
                    gsad::v1::ListDatabasesRequest const& request) {
        EXPECT_EQ(request.parent(), expected_instance.FullName());
        return internal::MakePaginationRange<StreamRange<gsad::v1::Database>>(
            gsad::v1::ListDatabasesRequest{},
            [](gsad::v1::ListDatabasesRequest const&) {
              gsad::v1::ListDatabasesResponse response;
              return StatusOr<gsad::v1::ListDatabasesResponse>(response);
            },
            [](gsad::v1::ListDatabasesResponse const&) {
              return std::vector<gsad::v1::Database>{};
            });
      });

  spanner_admin::DatabaseAdminClient client(mock);
  auto status = CleanupStaleDatabases(client, "test-project", "test-instance",
                                      std::chrono::system_clock::now());
  EXPECT_THAT(status, IsOk());
}

TEST(CleanupStaleDatabases, ListError) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  spanner::Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce([&expected_instance](
                    gsad::v1::ListDatabasesRequest const& request) {
        EXPECT_EQ(request.parent(), expected_instance.FullName());
        return internal::MakePaginationRange<StreamRange<gsad::v1::Database>>(
            gsad::v1::ListDatabasesRequest{},
            [](gsad::v1::ListDatabasesRequest const&) {
              return StatusOr<gsad::v1::ListDatabasesResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](gsad::v1::ListDatabasesResponse const&) {
              return std::vector<gsad::v1::Database>{};
            });
      });

  spanner_admin::DatabaseAdminClient client(mock);
  auto status = CleanupStaleDatabases(client, "test-project", "test-instance",
                                      std::chrono::system_clock::now());
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(CleanupStaleDatabases, RemovesMatching) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  auto generator = internal::DefaultPRNG({});
  auto const now = std::chrono::system_clock::now();
  auto const day = std::chrono::hours(24);
  std::vector<std::string> expect_dropped{
      RandomDatabaseName(generator, now - 5 * day),
      RandomDatabaseName(generator, now - 3 * day),
      RandomDatabaseName(generator, now - 3 * day),
      RandomDatabaseName(generator, now - 3 * day),
  };
  std::vector<std::string> expect_not_dropped{
      RandomDatabaseName(generator, now - 1 * day),
      RandomDatabaseName(generator, now - 1 * day),
      RandomDatabaseName(generator, now),
      RandomDatabaseName(generator, now),
      "db-not-matching-regex",
      "not-matching-prefix",
  };

  spanner::Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce([&](gsad::v1::ListDatabasesRequest const& request) {
        EXPECT_EQ(request.parent(), expected_instance.FullName());
        return internal::MakePaginationRange<StreamRange<gsad::v1::Database>>(
            gsad::v1::ListDatabasesRequest{},
            [&](gsad::v1::ListDatabasesRequest const&) {
              gsad::v1::ListDatabasesResponse response;
              for (auto const& id : expect_dropped) {
                gsad::v1::Database db;
                db.set_name(
                    spanner::Database(expected_instance, id).FullName());
                *response.add_databases() = std::move(db);
              }
              for (auto const& id : expect_not_dropped) {
                gsad::v1::Database db;
                db.set_name(
                    spanner::Database(expected_instance, id).FullName());
                *response.add_databases() = std::move(db);
              }
              return StatusOr<gsad::v1::ListDatabasesResponse>(response);
            },
            [](gsad::v1::ListDatabasesResponse const& r) {
              return std::vector<gsad::v1::Database>{r.databases().begin(),
                                                     r.databases().end()};
            });
      });

  std::vector<std::string> dropped;
  EXPECT_CALL(*mock, DropDatabase)
      .WillRepeatedly([&](gsad::v1::DropDatabaseRequest const& request) {
        auto db = spanner::MakeDatabase(request.database()).value();
        EXPECT_EQ(expected_instance, db.instance());
        dropped.push_back(db.database_id());
        // Errors should be ignored and the loop should continue.
        return Status{StatusCode::kPermissionDenied, "uh-oh"};
      });

  spanner_admin::DatabaseAdminClient client(mock);
  auto status = CleanupStaleDatabases(client, "test-project", "test-instance",
                                      now - 2 * day);
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(dropped, UnorderedElementsAreArray(expect_dropped.begin(),
                                                 expect_dropped.end()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
