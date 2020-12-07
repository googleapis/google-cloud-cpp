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

#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::Contains;
using ::testing::EndsWith;
using ::testing::HasSubstr;

class DatabaseAdminClientTest : public ::testing::Test {
 protected:
  // We can't use ASSERT* in the constructor, so defer initializing `instance_`
  // and `database_` until `SetUp()`.
  DatabaseAdminClientTest()
      : instance_("dummy", "dummy"), database_(instance_, "dummy") {}

  void SetUp() override {
    using ::google::cloud::internal::GetEnv;
    emulator_ = GetEnv("SPANNER_EMULATOR_HOST").has_value();
    auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id.empty());

    auto generator = google::cloud::internal::MakeDefaultPRNG();
    auto instance_id =
        spanner_testing::PickRandomInstance(generator, project_id);
    ASSERT_STATUS_OK(instance_id);
    instance_ = Instance(project_id, *instance_id);

    database_ =
        Database(instance_, spanner_testing::RandomDatabaseName(generator));

    test_iam_service_account_ =
        GetEnv("GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT").value_or("");
    ASSERT_TRUE(emulator_ || !test_iam_service_account_.empty());
  }

  // Does `database_` exist in `instance_`?
  bool DatabaseExists() {
    auto full_name = database_.FullName();
    for (auto const& database : client_.ListDatabases(instance_)) {
      EXPECT_STATUS_OK(database);
      if (!database) break;
      if (database->name() == full_name) return true;
    }
    return false;
  };

  Instance instance_;
  Database database_;
  DatabaseAdminClient client_;
  bool emulator_;
  std::string test_iam_service_account_;
};

/// @test Verify the basic CRUD operations for databases work.
TEST_F(DatabaseAdminClientTest, DatabaseBasicCRUD) {
  // We test Client::ListDatabases() by verifying that (a) it does not return a
  // randomly generated database name before we create a database with that
  // name, (b) it *does* return that database name once created, and (c) it no
  // longer returns that name once the database is dropped. Implicitly that also
  // tests that Client::DropDatabase() and client::CreateDatabase() do
  // something, which is nice.
  EXPECT_FALSE(DatabaseExists()) << "Database " << database_
                                 << " already exists, this is unexpected as "
                                    "the database id is selected at random.";

  auto database_future =
      client_.CreateDatabase(database_, /*extra_statements=*/{});
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  EXPECT_THAT(database->name(), EndsWith(database_.database_id()));

  auto get_result = client_.GetDatabase(database_);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(database->name(), get_result->name());

  if (!emulator_) {
    auto current_policy = client_.GetIamPolicy(database_);
    ASSERT_STATUS_OK(current_policy);
    EXPECT_EQ(0, current_policy->bindings_size());

    std::string const reader_role = "roles/spanner.databaseReader";
    std::string const writer_role = "roles/spanner.databaseUser";
    std::string const expected_member =
        "serviceAccount:" + test_iam_service_account_;
    auto& binding = *current_policy->add_bindings();
    binding.set_role(reader_role);
    *binding.add_members() = expected_member;

    auto updated_policy = client_.SetIamPolicy(database_, *current_policy);
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_EQ(1, updated_policy->bindings_size());
    ASSERT_EQ(reader_role, updated_policy->bindings().Get(0).role());
    ASSERT_EQ(1, updated_policy->bindings().Get(0).members().size());
    ASSERT_EQ(expected_member,
              updated_policy->bindings().Get(0).members().Get(0));

    // Perform a different update using the the OCC loop API:
    updated_policy = client_.SetIamPolicy(
        database_, [this, &writer_role](google::iam::v1::Policy current) {
          std::string const expected_member =
              "serviceAccount:" + test_iam_service_account_;
          auto& binding = *current.add_bindings();
          binding.set_role(writer_role);
          *binding.add_members() = expected_member;
          return current;
        });
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_EQ(2, updated_policy->bindings_size());
    ASSERT_EQ(writer_role, updated_policy->bindings().Get(1).role());
    ASSERT_EQ(1, updated_policy->bindings().Get(1).members().size());
    ASSERT_EQ(expected_member,
              updated_policy->bindings().Get(1).members().Get(0));

    // Fetch the Iam Policy again.
    current_policy = client_.GetIamPolicy(database_);
    ASSERT_STATUS_OK(current_policy);
    EXPECT_THAT(*updated_policy, IsProtoEqual(*current_policy));

    auto test_iam_permission_result =
        client_.TestIamPermissions(database_, {"spanner.databases.read"});
    ASSERT_STATUS_OK(test_iam_permission_result);
    ASSERT_EQ(1, test_iam_permission_result->permissions_size());
    ASSERT_EQ("spanner.databases.read",
              test_iam_permission_result->permissions(0));
  }

  auto get_ddl_result = client_.GetDatabaseDdl(database_);
  ASSERT_STATUS_OK(get_ddl_result);
  EXPECT_EQ(0, get_ddl_result->statements_size());

  std::vector<std::string> statements;
  statements.emplace_back(R"""(
        CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024),
          SingerInfo BYTES(MAX)
        ) PRIMARY KEY (SingerId)
      )""");
  auto update_future = client_.UpdateDatabase(database_, statements);
  auto metadata = update_future.get();
  ASSERT_STATUS_OK(metadata);
  EXPECT_THAT(metadata->database(), EndsWith(database_.database_id()));
  EXPECT_EQ(statements.size(), metadata->statements_size());
  EXPECT_EQ(statements.size(), metadata->commit_timestamps_size());
  if (metadata->statements_size() >= 1) {
    EXPECT_THAT(metadata->statements(), Contains(HasSubstr("CREATE TABLE")));
  }
  EXPECT_FALSE(metadata->throttled());

  EXPECT_TRUE(DatabaseExists()) << "Database " << database_;
  auto drop_status = client_.DropDatabase(database_);
  EXPECT_STATUS_OK(drop_status);
  EXPECT_FALSE(DatabaseExists()) << "Database " << database_;
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
