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

#include "google/cloud/bigtable/testing/inprocess_admin_client.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

namespace btadmin = google::bigtable::admin::v2;

grpc::Status InProcessAdminClient::CreateTable(
    grpc::ClientContext* context, btadmin::CreateTableRequest const& request,
    btadmin::Table* response) {
  return Stub()->CreateTable(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
InProcessAdminClient::AsyncCreateTable(
    grpc::ClientContext* context, btadmin::CreateTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncCreateTable(context, request, cq);
}

grpc::Status InProcessAdminClient::CreateTableFromSnapshot(
    grpc::ClientContext* context,
    btadmin::CreateTableFromSnapshotRequest const& request,
    google::longrunning::Operation* response) {
  return Stub()->CreateTableFromSnapshot(context, request, response);
}

grpc::Status InProcessAdminClient::ListTables(
    grpc::ClientContext* context, btadmin::ListTablesRequest const& request,
    btadmin::ListTablesResponse* response) {
  return Stub()->ListTables(context, request, response);
}

grpc::Status InProcessAdminClient::GetTable(
    grpc::ClientContext* context, btadmin::GetTableRequest const& request,
    btadmin::Table* response) {
  return Stub()->GetTable(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
InProcessAdminClient::AsyncGetTable(grpc::ClientContext* context,
                                    btadmin::GetTableRequest const& request,
                                    grpc::CompletionQueue* cq) {
  return Stub()->AsyncGetTable(context, request, cq);
}

grpc::Status InProcessAdminClient::DeleteTable(
    grpc::ClientContext* context, btadmin::DeleteTableRequest const& request,
    google::protobuf::Empty* response) {
  return Stub()->DeleteTable(context, request, response);
}

grpc::Status InProcessAdminClient::ModifyColumnFamilies(
    grpc::ClientContext* context,
    btadmin::ModifyColumnFamiliesRequest const& request,
    btadmin::Table* response) {
  return Stub()->ModifyColumnFamilies(context, request, response);
}

grpc::Status InProcessAdminClient::DropRowRange(
    grpc::ClientContext* context, btadmin::DropRowRangeRequest const& request,
    google::protobuf::Empty* response) {
  return Stub()->DropRowRange(context, request, response);
}

grpc::Status InProcessAdminClient::GenerateConsistencyToken(
    grpc::ClientContext* context,
    btadmin::GenerateConsistencyTokenRequest const& request,
    btadmin::GenerateConsistencyTokenResponse* response) {
  return Stub()->GenerateConsistencyToken(context, request, response);
}

grpc::Status InProcessAdminClient::CheckConsistency(
    grpc::ClientContext* context,
    btadmin::CheckConsistencyRequest const& request,
    btadmin::CheckConsistencyResponse* response) {
  return Stub()->CheckConsistency(context, request, response);
}

grpc::Status InProcessAdminClient::SnapshotTable(
    grpc::ClientContext* context, btadmin::SnapshotTableRequest const& request,
    google::longrunning::Operation* response) {
  return Stub()->SnapshotTable(context, request, response);
}

grpc::Status InProcessAdminClient::GetSnapshot(
    grpc::ClientContext* context, btadmin::GetSnapshotRequest const& request,
    btadmin::Snapshot* response) {
  return Stub()->GetSnapshot(context, request, response);
}

grpc::Status InProcessAdminClient::ListSnapshots(
    grpc::ClientContext* context, btadmin::ListSnapshotsRequest const& request,
    btadmin::ListSnapshotsResponse* response) {
  return Stub()->ListSnapshots(context, request, response);
}

grpc::Status InProcessAdminClient::DeleteSnapshot(
    grpc::ClientContext* context, btadmin::DeleteSnapshotRequest const& request,
    google::protobuf::Empty* response) {
  return Stub()->DeleteSnapshot(context, request, response);
}

grpc::Status InProcessAdminClient::GetOperation(
    grpc::ClientContext* context,
    google::longrunning::GetOperationRequest const& request,
    google::longrunning::Operation* response) {
  auto stub = google::longrunning::Operations::NewStub(Channel());
  return stub->GetOperation(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
InProcessAdminClient::AsyncModifyColumnFamilies(
    grpc::ClientContext* context,
    btadmin::ModifyColumnFamiliesRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncModifyColumnFamilies(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
InProcessAdminClient::AsyncDropRowRange(
    grpc::ClientContext* context, btadmin::DropRowRangeRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncDropRowRange(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Snapshot>>
InProcessAdminClient::AsyncGetSnapshot(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::GetSnapshotRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncGetSnapshot(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
InProcessAdminClient::AsyncDeleteSnapshot(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DeleteSnapshotRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncDeleteSnapshot(context, request, cq);
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
