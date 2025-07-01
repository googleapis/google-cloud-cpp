// Copyright 2024 Google LLC
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

#include "google/cloud/bigtable/emulator/server.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/admin/v2/bigtable_table_admin.pb.h>
#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/bigtable/v2/bigtable.pb.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

class ServerTest : public ::testing::Test {
 protected:
  std::unique_ptr<EmulatorServer> server_;
  std::shared_ptr<grpc::Channel> channel_;
  grpc::ClientContext ctx_;

  void SetUp() override {
    server_ = CreateDefaultEmulatorServer("127.0.0.1", 0);
    channel_ = grpc::CreateChannel(
        "localhost:" + std::to_string(server_->bound_port()),
        grpc::InsecureChannelCredentials());
  }

  std::unique_ptr<google::bigtable::v2::Bigtable::Stub> DataClient() {
    return google::bigtable::v2::Bigtable::NewStub(channel_);
  }

  std::unique_ptr<google::bigtable::admin::v2::BigtableTableAdmin::Stub>
  TableAdminClient() {
    return google::bigtable::admin::v2::BigtableTableAdmin::NewStub(channel_);
  }
};

TEST_F(ServerTest, DataCheckAndMutateRow) {
  google::bigtable::v2::CheckAndMutateRowRequest request;
  google::bigtable::v2::CheckAndMutateRowResponse response;

  grpc::Status status =
      DataClient()->CheckAndMutateRow(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataExecuteQuery) {
  google::bigtable::v2::ExecuteQueryRequest request;

  grpc::Status status = DataClient()->ExecuteQuery(&ctx_, request)->Finish();
  GTEST_SKIP() << "Data API's ExecuteQuery is not supported by the emulator.";
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataGenerateInitialChangeStreamPartitions) {
  google::bigtable::v2::GenerateInitialChangeStreamPartitionsRequest request;

  grpc::Status status =
      DataClient()
          ->GenerateInitialChangeStreamPartitions(&ctx_, request)
          ->Finish();
  GTEST_SKIP() << "Data API's GenerateInitialChangeStreamPartitions is not "
                  "supported by the emulator.";
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataMutateRow) {
  google::bigtable::v2::MutateRowRequest request;
  google::bigtable::v2::MutateRowResponse response;

  grpc::Status status = DataClient()->MutateRow(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataMutateRows) {
  google::bigtable::v2::MutateRowsRequest request;

  grpc::Status status = DataClient()->MutateRows(&ctx_, request)->Finish();
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataPingAndWarm) {
  google::bigtable::v2::PingAndWarmRequest request;
  google::bigtable::v2::PingAndWarmResponse response;

  grpc::Status status = DataClient()->PingAndWarm(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataReadChangeStream) {
  google::bigtable::v2::ReadChangeStreamRequest request;

  grpc::Status status =
      DataClient()->ReadChangeStream(&ctx_, request)->Finish();
  GTEST_SKIP()
      << "Data API's ReadChangeStream is not supported by the emulator.";
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataReadModifyWriteRow) {
  google::bigtable::v2::ReadModifyWriteRowRequest request;
  google::bigtable::v2::ReadModifyWriteRowResponse response;

  grpc::Status status =
      DataClient()->ReadModifyWriteRow(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataReadRows) {
  google::bigtable::v2::ReadRowsRequest request;

  grpc::Status status = DataClient()->ReadRows(&ctx_, request)->Finish();
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, DataSampleRowKeys) {
  google::bigtable::v2::SampleRowKeysRequest request;
  google::bigtable::v2::SampleRowKeysResponse response;

  grpc::Status status = DataClient()->SampleRowKeys(&ctx_, request)->Finish();
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminCheckConsistency) {
  google::bigtable::admin::v2::CheckConsistencyRequest request;
  google::bigtable::admin::v2::CheckConsistencyResponse response;

  grpc::Status status =
      TableAdminClient()->CheckConsistency(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminCreateTable) {
  google::bigtable::admin::v2::CreateTableRequest request;
  google::bigtable::admin::v2::Table response;

  grpc::Status status =
      TableAdminClient()->CreateTable(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminDeleteTable) {
  google::bigtable::admin::v2::DeleteTableRequest request;
  google::protobuf::Empty response;

  grpc::Status status =
      TableAdminClient()->DeleteTable(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminDropRowRange) {
  google::bigtable::admin::v2::DropRowRangeRequest request;
  google::protobuf::Empty response;

  grpc::Status status =
      TableAdminClient()->DropRowRange(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminGenerateConsistencyToken) {
  google::bigtable::admin::v2::GenerateConsistencyTokenRequest request;
  google::bigtable::admin::v2::GenerateConsistencyTokenResponse response;

  grpc::Status status =
      TableAdminClient()->GenerateConsistencyToken(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminGetTable) {
  google::bigtable::admin::v2::GetTableRequest request;
  google::bigtable::admin::v2::Table response;

  grpc::Status status = TableAdminClient()->GetTable(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminListTables) {
  google::bigtable::admin::v2::ListTablesRequest request;
  google::bigtable::admin::v2::ListTablesResponse response;

  grpc::Status status =
      TableAdminClient()->ListTables(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminModifyColumnFamilies) {
  google::bigtable::admin::v2::ModifyColumnFamiliesRequest request;
  google::bigtable::admin::v2::Table response;

  grpc::Status status =
      TableAdminClient()->ModifyColumnFamilies(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

TEST_F(ServerTest, TableAdminUpdateTable) {
  google::bigtable::admin::v2::UpdateTableRequest request;
  google::longrunning::Operation response;

  grpc::Status status =
      TableAdminClient()->UpdateTable(&ctx_, request, &response);
  EXPECT_NE(status.error_code(), grpc::StatusCode::UNIMPLEMENTED);
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
