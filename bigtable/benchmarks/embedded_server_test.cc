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

#include "bigtable/benchmarks/embedded_server.h"

#include <thread>

#include <gmock/gmock.h>

#include "bigtable/admin/table_admin.h"
#include "bigtable/client/table.h"

using namespace bigtable::benchmarks;

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

  bigtable::ClientOptions options;
  options.set_admin_endpoint(server->address());
  options.SetCredentials(grpc::InsecureChannelCredentials());
  bigtable::TableAdmin admin(
      bigtable::CreateDefaultAdminClient("fake-project", options),
      "fake-instance");

  auto gc = bigtable::GcRule::MaxNumVersions(42);
  EXPECT_NO_THROW(admin.CreateTable("fake-table-01",
                                    bigtable::TableConfig({{"fam", gc}}, {})));
  EXPECT_NO_THROW(admin.DeleteTable("fake-table-02"));

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, TableApply) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  bigtable::ClientOptions options;
  options.set_data_endpoint(server->address());
  options.SetCredentials(grpc::InsecureChannelCredentials());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  bigtable::SingleRowMutation mutation(
      "row1", {bigtable::SetCell("fam", "col", 0, "val"),
               bigtable::SetCell("fam", "col", 0, "val")});

  EXPECT_NO_THROW(table.Apply(std::move(mutation)));

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, TableBulkApply) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  bigtable::ClientOptions options;
  options.set_data_endpoint(server->address());
  options.SetCredentials(grpc::InsecureChannelCredentials());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  bigtable::BulkMutation bulk;
  bulk.emplace_back(bigtable::SingleRowMutation(
      "row1", {bigtable::SetCell("fam", "col", 0, "val")}));
  bulk.emplace_back(bigtable::SingleRowMutation(
      "row2", {bigtable::SetCell("fam", "col", 0, "val")}));

  EXPECT_NO_THROW(table.BulkApply(std::move(bulk)));

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, ReadRows1) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  bigtable::ClientOptions options;
  options.set_data_endpoint(server->address());
  options.SetCredentials(grpc::InsecureChannelCredentials());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  auto reader = table.ReadRows(bigtable::RowSet("row1"), 1,
                               bigtable::Filter::PassAllFilter());
  auto count = std::distance(reader.begin(), reader.end());
  EXPECT_EQ(1, count);

  server->Shutdown();
  wait_thread.join();
}

TEST(EmbeddedServer, ReadRows100) {
  auto server = CreateEmbeddedServer();
  std::thread wait_thread([&server]() { server->Wait(); });

  bigtable::ClientOptions options;
  options.set_data_endpoint(server->address());
  options.SetCredentials(grpc::InsecureChannelCredentials());
  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table");

  try {
    auto reader =
        table.ReadRows(bigtable::RowSet(bigtable::RowRange::StartingAt("foo")),
                       100, bigtable::Filter::PassAllFilter());
    auto count = std::distance(reader.begin(), reader.end());
    EXPECT_EQ(100, count);
  } catch(std::exception const& ex) {
    std::cerr << ex.what() << std::endl;
  }

  server->Shutdown();
  wait_thread.join();
}
