// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_EMBEDDED_SERVER_TEST_FIXTURE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_EMBEDDED_SERVER_TEST_FIXTURE_H

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/testing/inprocess_admin_client.h"
#include "google/cloud/bigtable/testing/inprocess_data_client.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <gtest/gtest.h>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

using ReceivedMetadata = std::multimap<std::string, std::string>;

inline void GetClientMetadata(grpc::ServerContext* context,
                              ReceivedMetadata& client_metadata) {
  for (auto const& kv : context->client_metadata()) {
    auto ele = std::make_pair(std::string(kv.first.begin(), kv.first.end()),
                              std::string(kv.second.begin(), kv.second.end()));
    client_metadata.emplace(std::move(ele));
  }
}

/**
 * Implement the portions of the `google.bigtable.v2.Bigtable` interface
 * necessary for the embedded server tests.
 *
 * This is not a Mock (use `google::bigtable::v2::MockBigtableStub` for that,
 * nor is this a Fake implementation (use the Cloud Bigtable Emulator for that),
 * this is an implementation of the interface that returns hardcoded values.
 * It is suitable for the embedded server tests, but for nothing else.
 */
class BigtableImpl final : public google::bigtable::v2::Bigtable::Service {
 public:
  BigtableImpl() {}
  grpc::Status ReadRows(
      grpc::ServerContext* context,
      google::bigtable::v2::ReadRowsRequest const*,
      grpc::ServerWriter<google::bigtable::v2::ReadRowsResponse>*) {
    GetClientMetadata(context, client_metadata_);
    return grpc::Status::OK;
  }
  const ReceivedMetadata& client_metadata() const { return client_metadata_; }

 private:
  ReceivedMetadata client_metadata_;
};

class TableAdminImpl final
    : public google::bigtable::admin::v2::BigtableTableAdmin::Service {
 public:
  TableAdminImpl() {}

  grpc::Status CreateTable(
      grpc::ServerContext* context,
      google::bigtable::admin::v2::CreateTableRequest const*,
      google::bigtable::admin::v2::Table*) override {
    GetClientMetadata(context, client_metadata_);
    return grpc::Status::OK;
  }
  grpc::Status GetTable(grpc::ServerContext* context,
                        google::bigtable::admin::v2::GetTableRequest const*,
                        google::bigtable::admin::v2::Table*) override {
    GetClientMetadata(context, client_metadata_);
    return grpc::Status::OK;
  }
  const ReceivedMetadata& client_metadata() const { return client_metadata_; }

 private:
  ReceivedMetadata client_metadata_;
};

/// Common fixture for integrating embedded server into tests.
class EmbeddedServerTestFixture : public ::testing::Test {
 protected:
  void StartServer();
  void SetUp();
  void TearDown();
  void ResetChannel();

  std::string const kProjectId = "foo-project";
  std::string const kInstanceId = "bar-instance";
  std::string const kTableId = "baz-table";
  std::string const kClusterId = "test_cluster";

  // These are hardcoded, and not computed, because we want to test the
  // computation.
  std::string const kInstanceName =
      "projects/foo-project/instances/bar-instance";
  std::string const kTableName =
      "projects/foo-project/instances/bar-instance/tables/baz-table";

  std::string project_id_ = kProjectId;
  std::string instance_id_ = kInstanceId;
  std::shared_ptr<DataClient> data_client_;
  std::shared_ptr<AdminClient> admin_client_;
  std::shared_ptr<bigtable::Table> table_;
  std::shared_ptr<bigtable::TableAdmin> admin_;
  std::thread wait_thread_;
  BigtableImpl bigtable_service_;
  TableAdminImpl admin_service_;
  grpc::ServerBuilder builder_;
  std::unique_ptr<grpc::Server> server_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_EMBEDDED_SERVER_TEST_FIXTURE_H
