// Copyright 2022 Google LLC
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

#include "google/cloud/spanner/internal/spanner_auth.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

SpannerAuth::SpannerAuth(
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    std::shared_ptr<SpannerStub> child)
    : auth_(std::move(auth)), child_(std::move(child)) {}

StatusOr<google::spanner::v1::Session> SpannerAuth::CreateSession(
    grpc::ClientContext& context,
    google::spanner::v1::CreateSessionRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->CreateSession(context, request);
}

StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
SpannerAuth::BatchCreateSessions(
    grpc::ClientContext& context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->BatchCreateSessions(context, request);
}

Status SpannerAuth::DeleteSession(
    grpc::ClientContext& context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->DeleteSession(context, request);
}

StatusOr<google::spanner::v1::ResultSet> SpannerAuth::ExecuteSql(
    grpc::ClientContext& context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ExecuteSql(context, request);
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
SpannerAuth::ExecuteStreamingSql(
    grpc::ClientContext& context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) {
    return absl::make_unique<ClientReaderInterfaceError>(std::move(status));
  }
  return child_->ExecuteStreamingSql(context, request);
}

StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>
SpannerAuth::ExecuteBatchDml(
    grpc::ClientContext& context,
    google::spanner::v1::ExecuteBatchDmlRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->ExecuteBatchDml(context, request);
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
SpannerAuth::StreamingRead(grpc::ClientContext& context,
                           google::spanner::v1::ReadRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) {
    return absl::make_unique<ClientReaderInterfaceError>(std::move(status));
  }
  return child_->StreamingRead(context, request);
}

StatusOr<google::spanner::v1::Transaction> SpannerAuth::BeginTransaction(
    grpc::ClientContext& context,
    google::spanner::v1::BeginTransactionRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->BeginTransaction(context, request);
}

StatusOr<google::spanner::v1::CommitResponse> SpannerAuth::Commit(
    grpc::ClientContext& context,
    google::spanner::v1::CommitRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->Commit(context, request);
}

Status SpannerAuth::Rollback(
    grpc::ClientContext& context,
    google::spanner::v1::RollbackRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->Rollback(context, request);
}

StatusOr<google::spanner::v1::PartitionResponse> SpannerAuth::PartitionQuery(
    grpc::ClientContext& context,
    google::spanner::v1::PartitionQueryRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PartitionQuery(context, request);
}

StatusOr<google::spanner::v1::PartitionResponse> SpannerAuth::PartitionRead(
    grpc::ClientContext& context,
    google::spanner::v1::PartitionReadRequest const& request) {
  auto status = auth_->ConfigureContext(context);
  if (!status.ok()) return status;
  return child_->PartitionRead(context, request);
}

future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
SpannerAuth::AsyncBatchCreateSessions(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  using ReturnType = StatusOr<google::spanner::v1::BatchCreateSessionsResponse>;
  auto& child = child_;
  return auth_->AsyncConfigureContext(std::move(context))
      .then([cq, child,
             request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                          f) mutable {
        auto context = f.get();
        if (!context) {
          return make_ready_future(ReturnType(std::move(context).status()));
        }
        return child->AsyncBatchCreateSessions(cq, *std::move(context),
                                               request);
      });
}

future<Status> SpannerAuth::AsyncDeleteSession(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  auto& child = child_;
  return auth_->AsyncConfigureContext(std::move(context))
      .then([cq, child,
             request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                          f) mutable {
        auto context = f.get();
        if (!context) return make_ready_future(std::move(context).status());
        return child->AsyncDeleteSession(cq, *std::move(context), request);
      });
}

future<StatusOr<google::spanner::v1::ResultSet>> SpannerAuth::AsyncExecuteSql(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  using ReturnType = StatusOr<google::spanner::v1::ResultSet>;
  auto& child = child_;
  return auth_->AsyncConfigureContext(std::move(context))
      .then([cq, child,
             request](future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
                          f) mutable {
        auto context = f.get();
        if (!context) {
          return make_ready_future(ReturnType(std::move(context).status()));
        }
        return child->AsyncExecuteSql(cq, *std::move(context), request);
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
