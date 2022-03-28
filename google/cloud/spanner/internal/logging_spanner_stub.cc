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

#include "google/cloud/spanner/internal/logging_spanner_stub.h"
#include "google/cloud/internal/log_wrapper.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::internal::LogWrapper;

StatusOr<google::spanner::v1::Session> LoggingSpannerStub::CreateSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::CreateSessionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::CreateSessionRequest const& request) {
        return child_->CreateSession(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
LoggingSpannerStub::BatchCreateSessions(
    grpc::ClientContext& client_context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::BatchCreateSessionsRequest const& request) {
        return child_->BatchCreateSessions(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
LoggingSpannerStub::AsyncBatchCreateSessions(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  return LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::spanner::v1::BatchCreateSessionsRequest const& request) {
        return child_->AsyncBatchCreateSessions(cq, std::move(context),
                                                request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::Session> LoggingSpannerStub::GetSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::GetSessionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::GetSessionRequest const& request) {
        return child_->GetSession(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::ListSessionsResponse>
LoggingSpannerStub::ListSessions(
    grpc::ClientContext& client_context,
    google::spanner::v1::ListSessionsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::ListSessionsRequest const& request) {
        return child_->ListSessions(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

Status LoggingSpannerStub::DeleteSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::DeleteSessionRequest const& request) {
        return child_->DeleteSession(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

future<Status> LoggingSpannerStub::AsyncDeleteSession(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  return LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::spanner::v1::DeleteSessionRequest const& request) {
        return child_->AsyncDeleteSession(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::ResultSet> LoggingSpannerStub::ExecuteSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::ExecuteSqlRequest const& request) {
        return child_->ExecuteSql(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

future<StatusOr<google::spanner::v1::ResultSet>>
LoggingSpannerStub::AsyncExecuteSql(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  return LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::spanner::v1::ExecuteSqlRequest const& request) {
        return child_->AsyncExecuteSql(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
LoggingSpannerStub::ExecuteStreamingSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::ExecuteSqlRequest const& request) {
        return child_->ExecuteStreamingSql(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>
LoggingSpannerStub::ExecuteBatchDml(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteBatchDmlRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::ExecuteBatchDmlRequest const& request) {
        return child_->ExecuteBatchDml(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
LoggingSpannerStub::StreamingRead(
    grpc::ClientContext& client_context,
    google::spanner::v1::ReadRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::ReadRequest const& request) {
        return child_->StreamingRead(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::Transaction> LoggingSpannerStub::BeginTransaction(
    grpc::ClientContext& client_context,
    google::spanner::v1::BeginTransactionRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::BeginTransactionRequest const& request) {
        return child_->BeginTransaction(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::CommitResponse> LoggingSpannerStub::Commit(
    grpc::ClientContext& client_context,
    google::spanner::v1::CommitRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::CommitRequest const& request) {
        return child_->Commit(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

Status LoggingSpannerStub::Rollback(
    grpc::ClientContext& client_context,
    google::spanner::v1::RollbackRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::RollbackRequest const& request) {
        return child_->Rollback(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::PartitionResponse>
LoggingSpannerStub::PartitionQuery(
    grpc::ClientContext& client_context,
    google::spanner::v1::PartitionQueryRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::PartitionQueryRequest const& request) {
        return child_->PartitionQuery(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::v1::PartitionResponse>
LoggingSpannerStub::PartitionRead(
    grpc::ClientContext& client_context,
    google::spanner::v1::PartitionReadRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::v1::PartitionReadRequest const& request) {
        return child_->PartitionRead(context, request);
      },
      client_context, request, __func__, tracing_options_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
