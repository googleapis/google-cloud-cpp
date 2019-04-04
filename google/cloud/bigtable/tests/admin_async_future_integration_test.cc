// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
namespace btadmin = google::bigtable::admin::v2;
using namespace google::cloud::testing_util::chrono_literals;

class AdminAsyncFutureIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  std::shared_ptr<AdminClient> admin_client_;
  std::unique_ptr<TableAdmin> table_admin_;
  std::unique_ptr<noex::TableAdmin> noex_table_admin_;

  void SetUp() {
    TableIntegrationTest::SetUp();
    admin_client_ = CreateDefaultAdminClient(
        testing::TableTestEnvironment::project_id(), ClientOptions());
    table_admin_ = google::cloud::internal::make_unique<TableAdmin>(
        admin_client_, bigtable::testing::TableTestEnvironment::instance_id());
    noex_table_admin_ = google::cloud::internal::make_unique<noex::TableAdmin>(
        admin_client_, bigtable::testing::TableTestEnvironment::instance_id());
  }

  void TearDown() {}

  int CountMatchingTables(std::string const& table_id,
                          std::vector<btadmin::Table> const& tables) {
    std::string table_name =
        table_admin_->instance_name() + "/tables/" + table_id;
    auto count = std::count_if(tables.begin(), tables.end(),
                               [&table_name](btadmin::Table const& t) {
                                 return table_name == t.name();
                               });
    return static_cast<int>(count);
  }
};

/// @test Verify that `bigtable::TableAdmin` Async CRUD operations work as
/// expected.
TEST_F(AdminAsyncFutureIntegrationTest, CreateListGetDeleteTableTest) {
  // Currently this test uses mostly synchronous operations, as we implement
  // async versions we should replace them in this function.

  std::string const table_id = RandomTableId();
  auto previous_table_list =
      table_admin_->ListTables(btadmin::Table::NAME_ONLY);
  ASSERT_STATUS_OK(previous_table_list);
  auto previous_count = CountMatchingTables(table_id, *previous_table_list);
  ASSERT_EQ(0, previous_count) << "Table (" << table_id << ") already exists."
                               << " This is unexpected, as the table ids are"
                               << " generated at random.";

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // AsyncCreateTable()
  TableConfig table_config({{"fam", GcRule::MaxNumVersions(5)},
                            {"foo", GcRule::MaxAge(std::chrono::hours(24))}},
                           {"a1000", "a2000", "b3000", "m5000"});

  auto count_matching_families = [](btadmin::Table const& table,
                                    std::string const& name) {
    int count = 0;
    for (auto const& kv : table.column_families()) {
      if (kv.first == name) {
        ++count;
      }
    }
    return count;
  };

  future<void> chain =
      table_admin_->AsyncCreateTable(cq, table_id, table_config)
          .then([&](future<StatusOr<btadmin::Table>> fut) {
            StatusOr<btadmin::Table> result = fut.get();
            EXPECT_STATUS_OK(result);
            EXPECT_THAT(result->name(), ::testing::HasSubstr(table_id));

            return table_admin_->AsyncGetTable(cq, table_id,
                                               btadmin::Table::FULL);
          })
          .then([&](future<StatusOr<btadmin::Table>> fut) {
            StatusOr<btadmin::Table> get_result = fut.get();
            EXPECT_STATUS_OK(get_result);

            EXPECT_EQ(1, count_matching_families(*get_result, "fam"));
            EXPECT_EQ(1, count_matching_families(*get_result, "foo"));

            // update table
            std::vector<bigtable::ColumnFamilyModification>
                column_modification_list = {
                    bigtable::ColumnFamilyModification::Create(
                        "newfam",
                        GcRule::Intersection(
                            GcRule::MaxAge(std::chrono::hours(7 * 24)),
                            GcRule::MaxNumVersions(1))),
                    bigtable::ColumnFamilyModification::Update(
                        "fam", GcRule::MaxNumVersions(2)),
                    bigtable::ColumnFamilyModification::Drop("foo")};
            return table_admin_->AsyncModifyColumnFamilies(
                cq, table_id, column_modification_list);
          })
          .then([&](future<StatusOr<btadmin::Table>> fut) {
            StatusOr<btadmin::Table> get_result = fut.get();
            EXPECT_EQ(1, count_matching_families(*get_result, "fam"));
            EXPECT_EQ(0, count_matching_families(*get_result, "foo"));
            EXPECT_EQ(1, count_matching_families(*get_result, "newfam"));
            auto const& gc =
                get_result->column_families().at("newfam").gc_rule();
            EXPECT_TRUE(gc.has_intersection());
            EXPECT_EQ(2, gc.intersection().rules_size());

            return table_admin_->AsyncDeleteTable(cq, table_id);
          })
          .then([&](future<Status> fut) {
            Status delete_result = fut.get();
            EXPECT_STATUS_OK(delete_result);
          });

  chain.get();
  SUCCEED();  // we expect that previous operations do not fail.

  cq.Shutdown();
  pool.join();
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableTestEnvironment(project_id,
                                                                 instance_id));

  return RUN_ALL_TESTS();
}
