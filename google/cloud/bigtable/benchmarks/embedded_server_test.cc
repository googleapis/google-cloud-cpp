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
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <thread>

namespace bigtable = google::cloud::bigtable;
using bigtable::benchmarks::CreateEmbeddedServer;
using std::chrono::milliseconds;

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

  bigtable::ClientOptions options(grpc::InsecureChannelCredentials());
  options.set_admin_endpoint(server->address());
  bigtable::TableAdmin admin(
      bigtable::CreateDefaultAdminClient("fake-project", options),
      "fake-instance");

  auto gc = bigtable::GcRule::MaxNumVersions(42);
  EXPECT_EQ(0, server->create_table_count());
  admin.CreateTable("fake-table-01", bigtable::TableConfig({{"fam", gc}}, {}));
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

  bigtable::ClientOptions options(grpc::InsecureChannelCredentials());
  options.set_data_endpoint(server->address());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  bigtable::SingleRowMutation mutation(
      "row1", {bigtable::SetCell("fam", "col", milliseconds(0), "val"),
               bigtable::SetCell("fam", "col", milliseconds(0), "val")});

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

  bigtable::ClientOptions options(grpc::InsecureChannelCredentials());
  options.set_data_endpoint(server->address());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  bigtable::BulkMutation bulk;
  bulk.emplace_back(bigtable::SingleRowMutation(
      "row1", {bigtable::SetCell("fam", "col", milliseconds(0), "val")}));
  bulk.emplace_back(bigtable::SingleRowMutation(
      "row2", {bigtable::SetCell("fam", "col", milliseconds(0), "val")}));

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

  bigtable::ClientOptions options(grpc::InsecureChannelCredentials());
  options.set_data_endpoint(server->address());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  EXPECT_EQ(0, server->read_rows_count());
  auto reader = table.ReadRows(bigtable::RowSet("row1"), 1,
                               bigtable::Filter::PassAllFilter());
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(1, count);
  EXPECT_EQ(1, server->read_rows_count());

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, ReadRows100) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  bigtable::ClientOptions options(grpc::InsecureChannelCredentials());
  options.set_data_endpoint(server->address());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  EXPECT_EQ(0, server->read_rows_count());
  auto reader =
      table.ReadRows(bigtable::RowSet(bigtable::RowRange::StartingAt("foo")),
                     100, bigtable::Filter::PassAllFilter());
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(100, count);
  EXPECT_EQ(1, server->read_rows_count());

  server->Shutdown();
  wait_thread.join();
}
