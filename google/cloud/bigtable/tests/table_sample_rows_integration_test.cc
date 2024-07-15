// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/row_key_sample.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::TableTestEnvironment;
using ::testing::IsEmpty;

Table GetTable() {
  return Table(MakeDataClient(TableTestEnvironment::project_id(),
                              TableTestEnvironment::instance_id()),
               TableTestEnvironment::table_id());
}

void VerifySamples(StatusOr<std::vector<RowKeySample>> samples) {
  ASSERT_STATUS_OK(samples);

  // It is somewhat hard to verify that the values returned here are correct.
  // We cannot check the specific values, not even the format, of the row keys
  // because Cloud Bigtable might return an empty row key (for "end of table"),
  // and it might return row keys that have never been written to.
  // All we can check is that this is not empty, and that the offsets are in
  // ascending order.
  ASSERT_THAT(*samples, Not(IsEmpty()));
  std::int64_t previous = 0;
  for (auto const& s : *samples) {
    EXPECT_LE(previous, s.offset_bytes);
    previous = s.offset_bytes;
  }
  // At least one of the samples should have non-zero offset:
  auto last = samples->back();
  EXPECT_LT(0, last.offset_bytes);
}

class SampleRowsIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 public:
  static void SetUpTestSuite() {
    // Create batch_size * batch_count rows. Use a special client with logging
    // disabled because it simply generates too much data.
    auto table =
        Table(MakeDataConnection(
                  Options{}
                      .set<LoggingComponentsOption>({"rpc"})
                      .set<GrpcTracingOptionsOption>(TracingOptions())),
              TableResource(TableTestEnvironment::project_id(),
                            TableTestEnvironment::instance_id(),
                            TableTestEnvironment::table_id()));

    int batch_count = 10;
    int batch_size = 5000;
    int column_count = 10;

    // The bigtable emulator is known to crash. Large bulk mutation requests
    // might be responsible.
    if (TableTestEnvironment::UsingCloudBigtableEmulator()) {
      batch_count = 1;
      batch_size = 50;
      column_count = 1;
    }

    std::string const family = "family1";
    int row_id = 0;

    for (int batch = 0; batch != batch_count; ++batch) {
      BulkMutation bulk;
      for (int row = 0; row != batch_size; ++row) {
        std::ostringstream os;
        os << "row:" << std::setw(9) << std::setfill('0') << row_id;

        // Build a mutation that creates N columns.
        SingleRowMutation mutation(os.str());
        for (int col = 0; col != column_count; ++col) {
          std::string column_id = "c" + std::to_string(col);
          std::string value = column_id + "#" + os.str();
          mutation.emplace_back(SetCell(family, std::move(column_id),
                                        std::chrono::milliseconds(0),
                                        std::move(value)));
        }
        bulk.emplace_back(std::move(mutation));
        ++row_id;
      }
      auto failures = table.BulkApply(std::move(bulk));
      ASSERT_THAT(failures, IsEmpty());
    }
  }
};

TEST_F(SampleRowsIntegrationTest, SyncWithDataConnection) {
  auto table = Table(MakeDataConnection(),
                     TableResource(TableTestEnvironment::project_id(),
                                   TableTestEnvironment::instance_id(),
                                   TableTestEnvironment::table_id()));
  VerifySamples(table.SampleRows());
};

TEST_F(SampleRowsIntegrationTest, AsyncWithDataConnection) {
  auto table = Table(MakeDataConnection(),
                     TableResource(TableTestEnvironment::project_id(),
                                   TableTestEnvironment::instance_id(),
                                   TableTestEnvironment::table_id()));
  auto fut = table.AsyncSampleRows();
  VerifySamples(fut.get());
};

TEST_F(SampleRowsIntegrationTest, SyncWithDataClient) {
  auto table = GetTable();
  VerifySamples(table.SampleRows());
};

TEST_F(SampleRowsIntegrationTest, AsyncWithDataClient) {
  auto table = GetTable();
  auto fut = table.AsyncSampleRows();

  // Block until the asynchronous operation completes. This is not what one
  // would do in a real application (the synchronous API is better in that
  // case), but we need to wait before checking the results.
  VerifySamples(fut.get());
};

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableTestEnvironment);

  return RUN_ALL_TESTS();
}
