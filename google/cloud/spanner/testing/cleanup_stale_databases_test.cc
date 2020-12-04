// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/testing/cleanup_stale_databases.h"
#include "google/cloud/spanner/mocks/mock_database_admin_connection.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::spanner_mocks::MockDatabaseAdminConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::UnorderedElementsAreArray;
namespace gcsa = ::google::spanner::admin::database::v1;
namespace spanner = ::google::cloud::spanner;

TEST(CleanupStaleDatabases, Empty) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  spanner::Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases(_))
      .WillOnce(
          [&expected_instance](
              spanner::DatabaseAdminConnection::ListDatabasesParams const& p) {
            EXPECT_EQ(expected_instance, p.instance);

            return google::cloud::internal::MakePaginationRange<
                google::cloud::spanner::ListDatabaseRange>(
                gcsa::ListDatabasesRequest{},
                [](gcsa::ListDatabasesRequest const&) {
                  gcsa::ListDatabasesResponse response;
                  return StatusOr<gcsa::ListDatabasesResponse>(response);
                },
                [](gcsa::ListDatabasesResponse const&) {
                  return std::vector<gcsa::Database>{};
                });
          });

  spanner::DatabaseAdminClient client(mock);
  auto status = CleanupStaleDatabases(client, "test-project", "test-instance",
                                      std::chrono::system_clock::now());
  EXPECT_STATUS_OK(status);
}

TEST(CleanupStaleDatabases, ListError) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  spanner::Instance const expected_instance("test-project", "test-instance");
  EXPECT_CALL(*mock, ListDatabases(_))
      .WillOnce(
          [&expected_instance](
              spanner::DatabaseAdminConnection::ListDatabasesParams const& p) {
            EXPECT_EQ(expected_instance, p.instance);

            return google::cloud::internal::MakePaginationRange<
                google::cloud::spanner::ListDatabaseRange>(
                gcsa::ListDatabasesRequest{},
                [](gcsa::ListDatabasesRequest const&) {
                  return StatusOr<gcsa::ListDatabasesResponse>(
                      Status(StatusCode::kPermissionDenied, "uh-oh"));
                },
                [](gcsa::ListDatabasesResponse const&) {
                  return std::vector<gcsa::Database>{};
                });
          });

  spanner::DatabaseAdminClient client(mock);
  auto status = CleanupStaleDatabases(client, "test-project", "test-instance",
                                      std::chrono::system_clock::now());
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(CleanupStaleDatabases, RemovesMatching) {
  auto mock = std::make_shared<MockDatabaseAdminConnection>();
  auto generator = google::cloud::internal::DefaultPRNG({});
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
  EXPECT_CALL(*mock, ListDatabases(_))
      .WillOnce(
          [&](spanner::DatabaseAdminConnection::ListDatabasesParams const& p) {
            EXPECT_EQ(expected_instance, p.instance);

            return google::cloud::internal::MakePaginationRange<
                google::cloud::spanner::ListDatabaseRange>(
                gcsa::ListDatabasesRequest{},
                [&](gcsa::ListDatabasesRequest const&) {
                  gcsa::ListDatabasesResponse response;
                  for (auto const& id : expect_dropped) {
                    gcsa::Database db;
                    db.set_name(
                        spanner::Database(expected_instance, id).FullName());
                    *response.add_databases() = std::move(db);
                  }
                  for (auto const& id : expect_not_dropped) {
                    gcsa::Database db;
                    db.set_name(
                        spanner::Database(expected_instance, id).FullName());
                    *response.add_databases() = std::move(db);
                  }
                  return StatusOr<gcsa::ListDatabasesResponse>(response);
                },
                [](gcsa::ListDatabasesResponse const& r) {
                  return std::vector<gcsa::Database>{r.databases().begin(),
                                                     r.databases().end()};
                });
          });

  std::vector<std::string> dropped;
  EXPECT_CALL(*mock, DropDatabase(_))
      .WillRepeatedly(
          [&](spanner::DatabaseAdminConnection::DropDatabaseParams const& p) {
            EXPECT_EQ(p.database.instance(), expected_instance);
            dropped.push_back(p.database.database_id());
            // Errors should be ignored and the loop should continue.
            return Status{StatusCode::kPermissionDenied, "uh-oh"};
          });

  spanner::DatabaseAdminClient client(mock);
  auto status = CleanupStaleDatabases(client, "test-project", "test-instance",
                                      now - 2 * day);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(expect_dropped,
              UnorderedElementsAreArray(dropped.begin(), dropped.end()));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
