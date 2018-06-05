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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_ADMIN_CLIENT_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/client_options.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/**
 * Connects to Cloud Bigtable's administration APIs.
 *
 * This class is mainly for testing purpose, it enable use of a single embedded
 * server
 * for multiple test cases. This adminlient uses a pre-defined channel.
 */
class InProcessAdminClient : public bigtable::AdminClient {
 public:
  InProcessAdminClient(std::string project,
                       std::shared_ptr<grpc::Channel> channel)
      : project_(std::move(project)), channel_(std::move(channel)) {}

  std::unique_ptr<
      google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>
  Stub() {
    return google::bigtable::admin::v2::BigtableTableAdmin::NewStub(channel_);
  }

  std::string const& project() const override { return project_; }
  std::shared_ptr<grpc::Channel> Channel() override { return channel_; }
  void reset() override {}

  //@{
  /// @name the google.bigtable.admin.v2.TableAdmin operations.
  virtual grpc::Status CreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      google::bigtable::admin::v2::Table* response) override;
  virtual grpc::Status CreateTableFromSnapshot(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableFromSnapshotRequest const&
          request,
      google::longrunning::Operation* response) override;
  virtual grpc::Status ListTables(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListTablesRequest const& request,
      google::bigtable::admin::v2::ListTablesResponse* response) override;
  virtual grpc::Status GetTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetTableRequest const& request,
      google::bigtable::admin::v2::Table* response) override;
  virtual grpc::Status DeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      google::protobuf::Empty* response) override;
  virtual grpc::Status ModifyColumnFamilies(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
      google::bigtable::admin::v2::Table* response) override;
  virtual grpc::Status DropRowRange(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DropRowRangeRequest const& request,
      google::protobuf::Empty* response) override;
  virtual grpc::Status GenerateConsistencyToken(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
          request,
      google::bigtable::admin::v2::GenerateConsistencyTokenResponse* response)
      override;
  virtual grpc::Status CheckConsistency(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CheckConsistencyRequest const& request,
      google::bigtable::admin::v2::CheckConsistencyResponse* response) override;
  virtual grpc::Status SnapshotTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::SnapshotTableRequest const& request,
      google::longrunning::Operation* response) override;
  virtual grpc::Status GetSnapshot(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetSnapshotRequest const& request,
      google::bigtable::admin::v2::Snapshot* response) override;
  virtual grpc::Status ListSnapshots(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListSnapshotsRequest const& request,
      google::bigtable::admin::v2::ListSnapshotsResponse* response) override;
  virtual grpc::Status DeleteSnapshot(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteSnapshotRequest const& request,
      google::protobuf::Empty* response) override;
  //@}

 private:
  std::string project_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_ADMIN_CLIENT_H_
