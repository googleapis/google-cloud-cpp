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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H_

#include "google/cloud/bigtable/admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockAdminClient : public bigtable::AdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD0(Channel, std::shared_ptr<grpc::Channel>());
  MOCK_METHOD0(reset, void());

  MOCK_METHOD3(
      CreateTable,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateTableRequest const& request,
          google::bigtable::admin::v2::Table* response));
  MOCK_METHOD3(
      AsyncCreateTable,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Table>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateTableRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(CreateTableFromSnapshot,
               grpc::Status(grpc::ClientContext* context,
                            google::bigtable::admin::v2::
                                CreateTableFromSnapshotRequest const& request,
                            google::longrunning::Operation* response));
  MOCK_METHOD3(
      ListTables,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ListTablesRequest const& request,
          google::bigtable::admin::v2::ListTablesResponse* response));
  MOCK_METHOD3(
      GetTable,
      grpc::Status(grpc::ClientContext* context,
                   google::bigtable::admin::v2::GetTableRequest const& request,
                   google::bigtable::admin::v2::Table* response));
  MOCK_METHOD3(AsyncGetTable,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::bigtable::admin::v2::Table>>(
                   grpc::ClientContext* context,
                   google::bigtable::admin::v2::GetTableRequest const& request,
                   grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      DeleteTable,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteTableRequest const& request,
          google::protobuf::Empty* response));
  MOCK_METHOD3(ModifyColumnFamilies,
               grpc::Status(grpc::ClientContext* context,
                            google::bigtable::admin::v2::
                                ModifyColumnFamiliesRequest const& request,
                            google::bigtable::admin::v2::Table* response));
  MOCK_METHOD3(
      DropRowRange,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DropRowRangeRequest const& request,
          google::protobuf::Empty* response));
  MOCK_METHOD3(
      GenerateConsistencyToken,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
              request,
          google::bigtable::admin::v2::GenerateConsistencyTokenResponse*
              response));
  MOCK_METHOD3(
      CheckConsistency,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CheckConsistencyRequest const& request,
          google::bigtable::admin::v2::CheckConsistencyResponse* response));
  MOCK_METHOD3(
      SnapshotTable,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::SnapshotTableRequest const& request,
          google::longrunning::Operation* response));
  MOCK_METHOD3(
      GetSnapshot,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetSnapshotRequest const& request,
          google::bigtable::admin::v2::Snapshot* response));
  MOCK_METHOD3(
      ListSnapshots,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ListSnapshotsRequest const& request,
          google::bigtable::admin::v2::ListSnapshotsResponse* response));
  MOCK_METHOD3(
      DeleteSnapshot,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteSnapshotRequest const& request,
          google::protobuf::Empty* response));
  MOCK_METHOD3(
      GetOperation,
      grpc::Status(grpc::ClientContext* context,
                   google::longrunning::GetOperationRequest const& request,
                   google::longrunning::Operation* response));
  MOCK_METHOD3(
      AsyncModifyColumnFamilies,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Table>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ModifyColumnFamiliesRequest const&
              request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncDropRowRange,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DropRowRangeRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncGenerateConsistencyToken,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>(
          grpc::ClientContext*,
          const google::bigtable::admin::v2::GenerateConsistencyTokenRequest&,
          grpc::CompletionQueue*));
  MOCK_METHOD3(AsyncCheckConsistency,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::bigtable::admin::v2::CheckConsistencyResponse>>(
                   grpc::ClientContext*,
                   const google::bigtable::admin::v2::CheckConsistencyRequest&,
                   grpc::CompletionQueue*));
  MOCK_METHOD3(
      AsyncGetSnapshot,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Snapshot>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetSnapshotRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncDeleteSnapshot,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteSnapshotRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(AsyncGetOperation,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::longrunning::Operation>>(
                   grpc::ClientContext* context,
                   const google::longrunning::GetOperationRequest& request,
                   grpc::CompletionQueue* cq));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H_
