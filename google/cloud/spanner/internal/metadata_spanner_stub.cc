// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/internal/metadata_spanner_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/options.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

MetadataSpannerStub::MetadataSpannerStub(std::shared_ptr<SpannerStub> child,
                                         std::string resource_prefix_header)
    : child_(std::move(child)),
      api_client_header_(google::cloud::internal::ApiClientHeader()),
      resource_prefix_header_(std::move(resource_prefix_header)) {}

StatusOr<google::spanner::v1::Session> MetadataSpannerStub::CreateSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::CreateSessionRequest const& request) {
  SetMetadata(client_context, "database=" + request.database());
  return child_->CreateSession(client_context, request);
}

StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
MetadataSpannerStub::BatchCreateSessions(
    grpc::ClientContext& client_context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  SetMetadata(client_context, "database=" + request.database());
  return child_->BatchCreateSessions(client_context, request);
}

future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
MetadataSpannerStub::AsyncBatchCreateSessions(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  SetMetadata(*context, "database=" + request.database());
  return child_->AsyncBatchCreateSessions(cq, std::move(context), request);
}

Status MetadataSpannerStub::DeleteSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  SetMetadata(client_context, "name=" + request.name());
  return child_->DeleteSession(client_context, request);
}

future<Status> MetadataSpannerStub::AsyncDeleteSession(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  SetMetadata(*context, "name=" + request.name());
  return child_->AsyncDeleteSession(cq, std::move(context), request);
}

StatusOr<google::spanner::v1::ResultSet> MetadataSpannerStub::ExecuteSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->ExecuteSql(client_context, request);
}

future<StatusOr<google::spanner::v1::ResultSet>>
MetadataSpannerStub::AsyncExecuteSql(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  SetMetadata(*context, "session=" + request.session());
  return child_->AsyncExecuteSql(cq, std::move(context), request);
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
MetadataSpannerStub::ExecuteStreamingSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->ExecuteStreamingSql(client_context, request);
}

StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>
MetadataSpannerStub::ExecuteBatchDml(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteBatchDmlRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->ExecuteBatchDml(client_context, request);
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
MetadataSpannerStub::StreamingRead(
    grpc::ClientContext& client_context,
    google::spanner::v1::ReadRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->StreamingRead(client_context, request);
}

StatusOr<google::spanner::v1::Transaction>
MetadataSpannerStub::BeginTransaction(
    grpc::ClientContext& client_context,
    google::spanner::v1::BeginTransactionRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->BeginTransaction(client_context, request);
}

StatusOr<google::spanner::v1::CommitResponse> MetadataSpannerStub::Commit(
    grpc::ClientContext& client_context,
    google::spanner::v1::CommitRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->Commit(client_context, request);
}

Status MetadataSpannerStub::Rollback(
    grpc::ClientContext& client_context,
    google::spanner::v1::RollbackRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->Rollback(client_context, request);
}

StatusOr<google::spanner::v1::PartitionResponse>
MetadataSpannerStub::PartitionQuery(
    grpc::ClientContext& client_context,
    google::spanner::v1::PartitionQueryRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->PartitionQuery(client_context, request);
}

StatusOr<google::spanner::v1::PartitionResponse>
MetadataSpannerStub::PartitionRead(
    grpc::ClientContext& client_context,
    google::spanner::v1::PartitionReadRequest const& request) {
  SetMetadata(client_context, "session=" + request.session());
  return child_->PartitionRead(client_context, request);
}

void MetadataSpannerStub::SetMetadata(grpc::ClientContext& context,
                                      std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", api_client_header_);
  context.AddMetadata("google-cloud-resource-prefix", resource_prefix_header_);
  auto const& options = internal::CurrentOptions();
  if (options.has<UserProjectOption>()) {
    context.AddMetadata("x-goog-user-project",
                        options.get<UserProjectOption>());
  }
  auto const& authority = options.get<AuthorityOption>();
  if (!authority.empty()) context.set_authority(authority);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
