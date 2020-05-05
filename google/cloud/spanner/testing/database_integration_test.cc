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

#include "google/cloud/spanner/testing/database_integration_test.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

google::cloud::spanner::Database* DatabaseIntegrationTest::db_;
google::cloud::internal::DefaultPRNG* DatabaseIntegrationTest::generator_;

void DatabaseIntegrationTest::SetUpTestSuite() {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());
  generator_ = new google::cloud::internal::DefaultPRNG(
      google::cloud::internal::MakeDefaultPRNG());

  auto instance_id = PickRandomInstance(*generator_, project_id);
  ASSERT_STATUS_OK(instance_id);

  auto database_id = spanner_testing::RandomDatabaseName(*generator_);

  db_ = new spanner::Database(project_id, *instance_id, database_id);

  std::cout << "Creating database and table " << std::flush;
  spanner::DatabaseAdminClient admin_client;
  auto database_future =
      admin_client.CreateDatabase(*db_, {R"sql(CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024)
                             ) PRIMARY KEY (SingerId))sql",
                                         R"sql(CREATE TABLE DataTypes (
    Id STRING(256) NOT NULL,
    BoolValue BOOL,
    Int64Value INT64,
    Float64Value FLOAT64,
    StringValue STRING(1024),
    BytesValue BYTES(1024),
    TimestampValue TIMESTAMP,
    DateValue DATE,
    ArrayBoolValue ARRAY<BOOL>,
    ArrayInt64Value ARRAY<INT64>,
    ArrayFloat64Value ARRAY<FLOAT64>,
    ArrayStringValue ARRAY<STRING(1024)>,
    ArrayBytesValue ARRAY<BYTES(1024)>,
    ArrayTimestampValue ARRAY<TIMESTAMP>,
    ArrayDateValue ARRAY<DATE>
  ) PRIMARY KEY (Id))sql"});
  int i = 0;
  int const timeout = 120;
  while (++i < timeout) {
    auto status = database_future.wait_for(std::chrono::seconds(1));
    if (status == std::future_status::ready) break;
    std::cout << '.' << std::flush;
  }
  if (i >= timeout) {
    std::cout << "TIMEOUT\n";
    FAIL();
  }
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);
  std::cout << "DONE\n";
}

void DatabaseIntegrationTest::TearDownTestSuite() {
  spanner::DatabaseAdminClient admin_client;
  auto drop_status = admin_client.DropDatabase(*db_);
  EXPECT_STATUS_OK(drop_status);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
