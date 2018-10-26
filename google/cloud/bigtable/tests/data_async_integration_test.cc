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

TEST_F(DataAsyncIntegrationTest, TableBulkApply) {
  std::string const table_id = RandomTableId();
  auto sync_table = CreateTable(table_id, table_config);

  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::map<std::string, std::vector<bigtable::Cell>> created{
      {row_key1,
       {{row_key1, family, "c0", 1000, "v1000", {}},
        {row_key1, family, "c1", 2000, "v2000", {}}}},
      {row_key2,
       {{row_key2, family, "c0", 3000, "v1000", {}},
        {row_key2, family, "c0", 4000, "v1000", {}}}}};

  BulkMutation mut;
  for (auto const& row_cells : created) {
    auto const& row_key = row_cells.first;
    auto const& cells = row_cells.second;
    SingleRowMutation row_mut(row_key);
    for (auto const& c : cells) {
      row_mut.emplace_back(SetCell(
          c.family_name(), c.column_qualifier(),
          std::chrono::duration_cast<std::chrono::milliseconds>(c.timestamp()),
          c.value()));
    }
    mut.push_back(row_mut);
  }

  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  noex::Table table(data_client_, table_id);
  std::promise<void> done;
  table.AsyncBulkApply(
      std::move(mut), cq,
      [&done](CompletionQueue& cq, std::vector<FailedMutation>& failed,
              grpc::Status const& status) {
        done.set_value();
        EXPECT_EQ(0, failed.size());
        EXPECT_TRUE(status.ok());
      });

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  done.get_future().get();

  // Validate that the newly created cells are actually in the server.
  std::vector<bigtable::Cell> expected;
  for (auto const& row_cells : created) {
    auto const& cells = row_cells.second;
    for (auto const& c : cells) {
      expected.push_back(c);
    }
  }

  auto actual = ReadRows(*sync_table, bigtable::Filter::PassAllFilter());

  // Cleanup the thread running the completion queue event loop.
  cq.Shutdown();
  pool.join();
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataAsyncIntegrationTest, SampleRowKeys) {
  std::string const table_id = RandomTableId();
  auto sync_table = CreateTable(table_id, table_config);

  // Create BATCH_SIZE * BATCH_COUNT rows.
  constexpr int BATCH_COUNT = 10;
  constexpr int BATCH_SIZE = 5000;
  constexpr int COLUMN_COUNT = 10;
  int rowid = 0;
  for (int batch = 0; batch != BATCH_COUNT; ++batch) {
    bigtable::BulkMutation bulk;
    for (int row = 0; row != BATCH_SIZE; ++row) {
      std::ostringstream os;
      os << "row:" << std::setw(9) << std::setfill('0') << rowid;

      // Build a mutation that creates 10 columns.
      bigtable::SingleRowMutation mutation(os.str());
      for (int col = 0; col != COLUMN_COUNT; ++col) {
        std::string colid = "c" + std::to_string(col);
        std::string value = colid + "#" + os.str();
        mutation.emplace_back(
            bigtable::SetCell(family, std::move(colid), std::move(value)));
      }
      bulk.emplace_back(std::move(mutation));
      ++rowid;
    }
    sync_table->BulkApply(std::move(bulk));
  }
  CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  noex::Table table(data_client_, table_id);
  std::promise<std::vector<RowKeySample>> done;
  table.AsyncSampleRowKeys(
      cq, [&done](CompletionQueue& cq, std::vector<RowKeySample>& samples,
                  grpc::Status const& status) {
        EXPECT_TRUE(status.ok());
        done.set_value(std::move(samples));
      });

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  auto samples = done.get_future().get();

  cq.Shutdown();
  pool.join();
  DeleteTable(table_id);

  // It is somewhat hard to verify that the values returned here are correct.
  // We cannot check the specific values, not even the format, of the row keys
  // because Cloud Bigtable might return an empty row key (for "end of table"),
  // and it might return row keys that have never been written to.
  // All we can check is that this is not empty, and that the offsets are in
  // ascending order.
  EXPECT_FALSE(samples.empty());
  std::int64_t previous = 0;
  for (auto const& s : samples) {
    EXPECT_LE(previous, s.offset_bytes);
    previous = s.offset_bytes;
  }
  // At least one of the samples should have non-zero offset:
  auto last = samples.back();
  EXPECT_LT(0, last.offset_bytes);
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
