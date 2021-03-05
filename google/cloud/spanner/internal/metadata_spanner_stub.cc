// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/metadata_spanner_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

namespace spanner_proto = ::google::spanner::v1;

MetadataSpannerStub::MetadataSpannerStub(std::shared_ptr<SpannerStub> child,
                                         std::string resource_prefix_header)
    : child_(std::move(child)),
      api_client_header_(google::cloud::internal::ApiClientHeader()),
      resource_prefix_header_(std::move(resource_prefix_header)) {}

StatusOr<spanner_proto::Session> MetadataSpannerStub::CreateSession(
    grpc::ClientContext& client_context,
    spanner_proto::CreateSessionRequest const& request) {
  SetMetadata(client_context, "database=" + request.database());
  return child_->CreateSession(client_context, request);
}

StatusOr<spanner_proto::BatchCreateSessionsResponse>
MetadataSpannerStub::BatchCreateSessions(
    grpc::ClientContext& client_context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  SetMetadata(client_context, "database=" + request.database());
  return child_->BatchCreateSessions(client_context, request);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    spanner_proto::BatchCreateSessionsResponse>>
MetadataSpannerStub::AsyncBatchCreateSessions(
    grpc::ClientContext& client_context,
    spanner_proto::BatchCreateSessionsRequest const& request,
    grpc::CompletionQueue* cq) {
  SetMetadata(client_context, "database=" + request.database());
  return child_->AsyncBatchCreateSessions(client_context, request, cq);
}

StatusOr<spanner_proto::Session> MetadataSpannerStub::GetSession(
    grpc::ClientContext& client_context,
    spanner_proto::GetSessionRequest const& request) {
  SetMetadata(client_context, "name=" + request.name());
  return child_->GetSession(client_context, request);
}

StatusOr<spanner_proto::ListSessionsResponse> MetadataSpannerStub::ListSessions(
    grpc::ClientContext& client_context,
    spanner_proto::ListSessionsRequest const& request) {
  SetMetadata(client_context, "database=" + request.database());
  return child_->ListSessions(client_context, request);
}

Status MetadataSpannerStub::DeleteSession(
    grpc::ClientContext& client_context,
    spanner_proto::DeleteSessionRequest const& request) {
  SetMetadata(client_context, "name=" + request.name());
  return child_->DeleteSession(client_context, request);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
MetadataSpannerStub::AsyncDeleteSession(
    grpc::ClientContext& client_context,
    spanner_proto::DeleteSessionRequest const& request,
    grpc::CompletionQueue* cq) {
  SetMetadata(client_context, "name=" + request.name());
  return child_->AsyncDeleteSession(client_context, request, cq);
}

StatusOr<spanner_proto::ResultSet> MetadataSpannerStub::ExecuteSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->ExecuteSql(client_context, request);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<spanner_proto::ResultSet>>
MetadataSpannerStub::AsyncExecuteSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request,
    grpc::CompletionQueue* cq) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->AsyncExecuteSql(client_context, request, cq);
}

std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
MetadataSpannerStub::ExecuteStreamingSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->ExecuteStreamingSql(client_context, request);
}

StatusOr<spanner_proto::ExecuteBatchDmlResponse>
MetadataSpannerStub::ExecuteBatchDml(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteBatchDmlRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->ExecuteBatchDml(client_context, request);
}

std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
MetadataSpannerStub::StreamingRead(grpc::ClientContext& client_context,
                                   spanner_proto::ReadRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->StreamingRead(client_context, request);
}

StatusOr<spanner_proto::Transaction> MetadataSpannerStub::BeginTransaction(
    grpc::ClientContext& client_context,
    spanner_proto::BeginTransactionRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->BeginTransaction(client_context, request);
}

StatusOr<spanner_proto::CommitResponse> MetadataSpannerStub::Commit(
    grpc::ClientContext& client_context,
    spanner_proto::CommitRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->Commit(client_context, request);
}

Status MetadataSpannerStub::Rollback(
    grpc::ClientContext& client_context,
    spanner_proto::RollbackRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->Rollback(client_context, request);
}

StatusOr<spanner_proto::PartitionResponse> MetadataSpannerStub::PartitionQuery(
    grpc::ClientContext& client_context,
    spanner_proto::PartitionQueryRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->PartitionQuery(client_context, request);
}

StatusOr<spanner_proto::PartitionResponse> MetadataSpannerStub::PartitionRead(
    grpc::ClientContext& client_context,
    spanner_proto::PartitionReadRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->PartitionRead(client_context, request);
}

void MetadataSpannerStub::SetMetadata(grpc::ClientContext& context,
                                      std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", api_client_header_);
  context.AddMetadata("google-cloud-resource-prefix", resource_prefix_header_);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
