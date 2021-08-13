// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/embedded_server.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
namespace benchmarks {
namespace {

using ::std::chrono::milliseconds;

TEST(EmbeddedServer, WaitAndShutdown) {
  auto server = CreateEmbeddedServer();
  EXPECT_FALSE(server->address().empty());

  std::thread wait_thread([&server]() { server->Wait(); });
  EXPECT_TRUE(wait_thread.joinable());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_TRUE(wait_thread.joinable());
  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, Admin) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  ClientOptions options(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<AdminEndpointOption>(server->address()));

  TableAdmin admin(CreateDefaultAdminClient("fake-project", options),
                   "fake-instance");

  auto gc = GcRule::MaxNumVersions(42);
  EXPECT_EQ(0, server->create_table_count());
  admin.CreateTable("fake-table-01", TableConfig({{"fam", gc}}, {}));
  EXPECT_EQ(1, server->create_table_count());

  EXPECT_EQ(0, server->delete_table_count());
  admin.DeleteTable("fake-table-02");
  EXPECT_EQ(1, server->delete_table_count());

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, TableApply) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  ClientOptions options(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<DataEndpointOption>(server->address()));

  Table table(CreateDefaultDataClient("fake-project", "fake-instance", options),
              "fake-table");

  SingleRowMutation mutation("row1",
                             {SetCell("fam", "col", milliseconds(0), "val"),
                              SetCell("fam", "col", milliseconds(0), "val")});

  EXPECT_EQ(0, server->mutate_row_count());
  auto status = table.Apply(std::move(mutation));
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(1, server->mutate_row_count());

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, TableBulkApply) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  ClientOptions options(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<DataEndpointOption>(server->address()));

  Table table(CreateDefaultDataClient("fake-project", "fake-instance", options),
              "fake-table");

  BulkMutation bulk;
  bulk.emplace_back(SingleRowMutation(
      "row1", {SetCell("fam", "col", milliseconds(0), "val")}));
  bulk.emplace_back(SingleRowMutation(
      "row2", {SetCell("fam", "col", milliseconds(0), "val")}));

  EXPECT_EQ(0, server->mutate_rows_count());
  auto failures = table.BulkApply(std::move(bulk));
  EXPECT_TRUE(failures.empty());
  EXPECT_EQ(1, server->mutate_rows_count());

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, ReadRows1) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  ClientOptions options(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<DataEndpointOption>(server->address()));

  Table table(CreateDefaultDataClient("fake-project", "fake-instance", options),
              "fake-table");

  EXPECT_EQ(0, server->read_rows_count());
  auto reader = table.ReadRows(RowSet("row1"), 1, Filter::PassAllFilter());
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(1, count);
  EXPECT_EQ(1, server->read_rows_count());

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, ReadRows100) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  ClientOptions options(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<DataEndpointOption>(server->address()));

  Table table(CreateDefaultDataClient("fake-project", "fake-instance", options),
              "fake-table");

  EXPECT_EQ(0, server->read_rows_count());
  auto reader = table.ReadRows(RowSet(RowRange::StartingAt("foo")), 100,
                               Filter::PassAllFilter());
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(100, count);
  EXPECT_EQ(1, server->read_rows_count());

  server->Shutdown();
  wait_thread.join();
}

}  // namespace
}  // namespace benchmarks
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
