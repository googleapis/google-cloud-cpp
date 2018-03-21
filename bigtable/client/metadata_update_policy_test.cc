// Copyright 2018 Google Inc.
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

#include "bigtable/client/metadata_update_policy.h"
#include "bigtable/client/admin_client.h"
#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/table.h"
#include "bigtable/client/table_admin.h"
#include "google/bigtable/v2/bigtable.grpc.pb.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <gtest/gtest.h>
#include <thread>

std::string const kInstanceName = "projects/foo-project/instances/bar-instance";
std::string const kTableId = "baz-table";
std::string const kTableName =
    "projects/foo-project/instances/bar-instance/tables/baz-table";

namespace btproto = google::bigtable::v2;
namespace adminproto = google::bigtable::admin::v2;

/**
 * Implement the portions of the `google.bigtable.v2.Bigtable` interface
 * necessary for the embedded server tests.
 *
 * This is not a Mock (use `google::bigtable::v2::MockBigtableStub` for that,
 * nor is this a Fake implementation (use the Cloud Bigtable Emulator for that),
 * this is an implementation of the interface that returns hardcoded values.
 * It is suitable for the embedded server tests, but for nothing else.
 */
class BigtableImpl final : public btproto::Bigtable::Service {
 public:
  BigtableImpl() {}
  grpc::Status ReadRows(
      grpc::ServerContext* context,
      google::bigtable::v2::ReadRowsRequest const* request,
      grpc::ServerWriter<google::bigtable::v2::ReadRowsResponse>* writer)
      override {
    for (std::multimap<grpc::string_ref, grpc::string_ref>::const_iterator it =
             context->client_metadata().begin();
         it != context->client_metadata().end(); ++it) {
      auto ele = std::make_pair((*it).first, (*it).second);
      client_metadata_.emplace(ele);
    }
    return grpc::Status::OK;
  }

  const std::multimap<grpc::string_ref, grpc::string_ref>& client_metadata()
      const {
    return client_metadata_;
  }

 private:
  std::multimap<grpc::string_ref, grpc::string_ref> client_metadata_;
};

/**
 * Implement the `google.bigtable.admin.v2.BigtableTableAdmin` interface for the
 * embedded server tests.
 */
class TableAdminImpl final : public adminproto::BigtableTableAdmin::Service {
 public:
  TableAdminImpl() {}

  grpc::Status CreateTable(
      grpc::ServerContext* context,
      google::bigtable::admin::v2::CreateTableRequest const* request,
      google::bigtable::admin::v2::Table* response) override {
    for (std::multimap<grpc::string_ref, grpc::string_ref>::const_iterator it =
             context->client_metadata().begin();
         it != context->client_metadata().end(); ++it) {
      auto ele = std::make_pair((*it).first, (*it).second);
      client_metadata_.emplace(ele);
    }
    return grpc::Status::OK;
  }

  grpc::Status DeleteTable(
      grpc::ServerContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const* request,
      ::google::protobuf::Empty* response) override {
    for (std::multimap<grpc::string_ref, grpc::string_ref>::const_iterator it =
             context->client_metadata().begin();
         it != context->client_metadata().end(); ++it) {
      auto ele = std::make_pair((*it).first, (*it).second);
      client_metadata_.emplace(ele);
    }
    return grpc::Status::OK;
  }

  grpc::Status GetTable(
      grpc::ServerContext* context,
      google::bigtable::admin::v2::GetTableRequest const* request,
      google::bigtable::admin::v2::Table* response) override {
    for (std::multimap<grpc::string_ref, grpc::string_ref>::const_iterator it =
             context->client_metadata().begin();
         it != context->client_metadata().end(); ++it) {
      auto ele = std::make_pair((*it).first, (*it).second);
      client_metadata_.emplace(ele);
    }
    return grpc::Status::OK;
  }

  const std::multimap<grpc::string_ref, grpc::string_ref>& client_metadata()
      const {
    return client_metadata_;
  }

 private:
  std::multimap<grpc::string_ref, grpc::string_ref> client_metadata_;
};

/// The implementation of EmbeddedServer.
class DefaultEmbeddedServer {
 public:
  explicit DefaultEmbeddedServer() {
    int port;
    std::string server_address("[::]:0");
    builder_.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                              &port);
    builder_.RegisterService(&bigtable_service_);
    builder_.RegisterService(&admin_service_);
    server_ = builder_.BuildAndStart();
    address_ = "localhost:" + std::to_string(port);
  }

  std::string address() const { return address_; }
  void Shutdown() { server_->Shutdown(); }
  void Wait() { server_->Wait(); }
  const std::multimap<grpc::string_ref, grpc::string_ref>&
  admin_service_client_metadata() const {
    return admin_service_.client_metadata();
  }
  const std::multimap<grpc::string_ref, grpc::string_ref>&
  bigtable_service_client_metadata() const {
    return bigtable_service_.client_metadata();
  }

 private:
  BigtableImpl bigtable_service_;
  TableAdminImpl admin_service_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
  std::string address_;
};

/// @test A test for setting metadata when table is not known.
TEST(MetadataUpdatePolicy, RunWithEmbeddedServer) {
  std::unique_ptr<DefaultEmbeddedServer> server =
      bigtable::internal::make_unique<DefaultEmbeddedServer>();
  std::string address = server->address();
  EXPECT_FALSE(server->address().empty());
  bigtable::ClientOptions options;
  options.set_admin_endpoint(address);
  options.set_data_endpoint(address);
  options.SetCredentials(grpc::InsecureChannelCredentials());
  std::thread wait_thread([&server]() { server->Wait(); });
  EXPECT_TRUE(wait_thread.joinable());

  // Create the table against embedded server.
  bigtable::TableAdmin admin(
      bigtable::CreateDefaultAdminClient("fake-project", options),
      "fake-instance");
  grpc::string_ref expected_x_goog_request_param =
      "parent=projects/fake-project/instances/fake-instance";

  auto gc = bigtable::GcRule::MaxNumVersions(42);
  admin.CreateTable("fake-table-01", bigtable::TableConfig({{"fam", gc}}, {}));

  // Get the metadata from embedded server
  auto client_metadata = server->admin_service_client_metadata();
  std::pair<std::multimap<grpc::string_ref, grpc::string_ref>::iterator,
            std::multimap<grpc::string_ref, grpc::string_ref>::iterator>
      result;
  result = client_metadata.equal_range("x-goog-request-params");
  // first check to see if client_metadata has only one occurance of parameter.
  EXPECT_EQ(1, std::distance(result.first, result.second));

  for (std::multimap<grpc::string_ref, grpc::string_ref>::iterator it =
           result.first;
       it != result.second; ++it) {
    grpc::string_ref actual_x_goog_request_param = it->second;
    EXPECT_EQ(expected_x_goog_request_param, actual_x_goog_request_param);
  }
  EXPECT_TRUE(wait_thread.joinable());
  server->Shutdown();
  wait_thread.join();
}
/// @test A test for setting metadata when table_id is known during the call.
TEST(MetadataUpdatePolicy, RunWithEmbeddedServerLazyMetadata) {
  std::unique_ptr<DefaultEmbeddedServer> server =
      bigtable::internal::make_unique<DefaultEmbeddedServer>();
  std::string address = server->address();
  EXPECT_FALSE(server->address().empty());
  bigtable::ClientOptions options;
  options.set_admin_endpoint(address);
  options.set_data_endpoint(address);
  options.SetCredentials(grpc::InsecureChannelCredentials());
  std::thread wait_thread([&server]() { server->Wait(); });
  EXPECT_TRUE(wait_thread.joinable());

  // Create the table against embedded server.
  bigtable::TableAdmin admin(
      bigtable::CreateDefaultAdminClient("fake-project", options),
      "fake-instance");

  admin.GetTable("fake-table-01");

  grpc::string_ref expected_x_goog_request_param =
      "name=projects/fake-project/instances/fake-instance/tables/fake-table-01";
  // Get the metadata from embedded server
  auto client_metadata = server->admin_service_client_metadata();
  std::pair<std::multimap<grpc::string_ref, grpc::string_ref>::iterator,
            std::multimap<grpc::string_ref, grpc::string_ref>::iterator>
      result;
  result = client_metadata.equal_range("x-goog-request-params");
  // first check to see if client_metadata has only one occurance of parameter.
  EXPECT_EQ(1, std::distance(result.first, result.second));

  for (std::multimap<grpc::string_ref, grpc::string_ref>::iterator it =
           result.first;
       it != result.second; ++it) {
    grpc::string_ref actual_x_goog_request_param = it->second;
    EXPECT_EQ(expected_x_goog_request_param, actual_x_goog_request_param);
  }
  EXPECT_TRUE(wait_thread.joinable());
  server->Shutdown();
  wait_thread.join();
}

/// @test A test for setting metadata when table_id is known during the call.
TEST(MetadataUpdatePolicy, RunWithEmbeddedServerParamTableName) {
  std::unique_ptr<DefaultEmbeddedServer> server =
      bigtable::internal::make_unique<DefaultEmbeddedServer>();
  std::string address = server->address();
  EXPECT_FALSE(server->address().empty());
  bigtable::ClientOptions options;
  options.set_admin_endpoint(address);
  options.set_data_endpoint(address);
  options.SetCredentials(grpc::InsecureChannelCredentials());
  std::thread wait_thread([&server]() { server->Wait(); });
  EXPECT_TRUE(wait_thread.joinable());

  bigtable::Table table(bigtable::CreateDefaultDataClient(
                            "fake-project", "fake-instance", options),
                        "fake-table-01");

  grpc::string_ref expected_x_goog_request_param =
      "table_name=projects/fake-project/instances/fake-instance/tables/"
      "fake-table-01";

  auto reader = table.ReadRows(bigtable::RowSet("row1"), 1,
                               bigtable::Filter::PassAllFilter());
  // lets make the RPC call to send metadata
  reader.begin();

  // Get metadata from embedded server
  auto client_metadata = server->bigtable_service_client_metadata();
  std::pair<std::multimap<grpc::string_ref, grpc::string_ref>::iterator,
            std::multimap<grpc::string_ref, grpc::string_ref>::iterator>
      result;
  result = client_metadata.equal_range("x-goog-request-params");
  // first check to see if client_metadata has only one occurance of parameter.
  EXPECT_EQ(1, std::distance(result.first, result.second));

  for (std::multimap<grpc::string_ref, grpc::string_ref>::iterator it =
           result.first;
       it != result.second; ++it) {
    grpc::string_ref actual_x_goog_request_param = it->second;
    EXPECT_EQ(expected_x_goog_request_param, actual_x_goog_request_param);
  }
  EXPECT_TRUE(wait_thread.joinable());
  server->Shutdown();
  wait_thread.join();
}

/// @test A cloning test for normal construction of metadata .
TEST(MetadataUpdatePolicy, SimpleDefault) {
  auto const x_google_request_params = "parent=" + kInstanceName;
  bigtable::MetadataUpdatePolicy created(kInstanceName,
                                         bigtable::MetadataParamTypes::PARENT);
  EXPECT_EQ(x_google_request_params, created.x_google_request_params().second);
}

/// @test A test for lazy behaviour of metadata .
TEST(MetadataUpdatePolicy, SimpleLazy) {
  auto const x_google_request_params = "name=" + kTableName;
  bigtable::MetadataUpdatePolicy created(
      kInstanceName, bigtable::MetadataParamTypes::NAME, kTableId);
  EXPECT_EQ(x_google_request_params, created.x_google_request_params().second);
}
