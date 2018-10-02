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

#include "google/cloud/bigtable/internal/endian.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
class DataAsyncIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  std::string const family = "family";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{family, bigtable::GcRule::MaxNumVersions(10)}}, {});
};

using namespace google::cloud::testing_util::chrono_literals;

TEST_F(DataAsyncIntegrationTest, TableApply) {
  std::string const table_id = RandomTableId();
  auto sync_table = CreateTable(table_id, table_config);

  std::string const row_key = "row-key-1";
  std::vector<bigtable::Cell> created{
      {row_key, family, "c0", 1000, "v1000", {}},
      {row_key, family, "c1", 2000, "v2000", {}}};
  SingleRowMutation mut(row_key);
  for (auto const& c : created) {
    mut.emplace_back(SetCell(
        c.family_name(), c.column_qualifier(),
        std::chrono::duration_cast<std::chrono::milliseconds>(c.timestamp()),
        c.value()));
  }

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  noex::Table table(data_client_, table_id);
  std::promise<void> done;
  table.AsyncApply(
      std::move(mut), cq,
      [&done](CompletionQueue& cq, google::bigtable::v2::MutateRowResponse& r,
              grpc::Status const& status) {
        done.set_value();
        EXPECT_TRUE(status.ok());
      });

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  done.get_future().get();

  // Validate that the newly created cells are actually in the server.
  std::vector<bigtable::Cell> expected{
      {row_key, family, "c0", 1000, "v1000", {}},
      {row_key, family, "c1", 2000, "v2000", {}}};

  auto actual = ReadRows(*sync_table, bigtable::Filter::PassAllFilter());

  // Cleanup the thread running the completion queue event loop.
  cq.Shutdown();
  pool.join();
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
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
              << " <project> <instance>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableTestEnvironment(project_id,
                                                                 instance_id));

  return RUN_ALL_TESTS();
}
