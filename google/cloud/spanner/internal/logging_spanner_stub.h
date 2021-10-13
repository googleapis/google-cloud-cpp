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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_LOGGING_SPANNER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_LOGGING_SPANNER_STUB_H

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/spanner/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A SpannerStub that logs each request.
 */
class LoggingSpannerStub : public SpannerStub {
 public:
  LoggingSpannerStub(std::shared_ptr<SpannerStub> child,
                     TracingOptions tracing_options)
      : child_(std::move(child)),
        tracing_options_(std::move(tracing_options)) {}
  ~LoggingSpannerStub() override = default;

  StatusOr<google::spanner::v1::Session> CreateSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::CreateSessionRequest const& request) override;
  StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
  BatchCreateSessions(
      grpc::ClientContext& client_context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::spanner::v1::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(
      grpc::ClientContext& client_context,
      google::spanner::v1::BatchCreateSessionsRequest const& request,
      grpc::CompletionQueue* cq) override;
  StatusOr<google::spanner::v1::Session> GetSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::GetSessionRequest const& request) override;
  StatusOr<google::spanner::v1::ListSessionsResponse> ListSessions(
      grpc::ClientContext& client_context,
      google::spanner::v1::ListSessionsRequest const& request) override;
  Status DeleteSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::DeleteSessionRequest const& request) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteSession(grpc::ClientContext& client_context,
                     google::spanner::v1::DeleteSessionRequest const& request,
                     grpc::CompletionQueue* cq) override;
  StatusOr<google::spanner::v1::ResultSet> ExecuteSql(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::spanner::v1::ResultSet>>
  AsyncExecuteSql(grpc::ClientContext& client_context,
                  google::spanner::v1::ExecuteSqlRequest const& request,
                  grpc::CompletionQueue* cq) override;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
  ExecuteStreamingSql(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;
  StatusOr<google::spanner::v1::ExecuteBatchDmlResponse> ExecuteBatchDml(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteBatchDmlRequest const& request) override;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
  StreamingRead(grpc::ClientContext& client_context,
                google::spanner::v1::ReadRequest const& request) override;
  StatusOr<google::spanner::v1::Transaction> BeginTransaction(
      grpc::ClientContext& client_context,
      google::spanner::v1::BeginTransactionRequest const& request) override;
  StatusOr<google::spanner::v1::CommitResponse> Commit(
      grpc::ClientContext& client_context,
      google::spanner::v1::CommitRequest const& request) override;
  Status Rollback(grpc::ClientContext& client_context,
                  google::spanner::v1::RollbackRequest const& request) override;
  StatusOr<google::spanner::v1::PartitionResponse> PartitionQuery(
      grpc::ClientContext& client_context,
      google::spanner::v1::PartitionQueryRequest const& request) override;
  StatusOr<google::spanner::v1::PartitionResponse> PartitionRead(
      grpc::ClientContext& client_context,
      google::spanner::v1::PartitionReadRequest const& request) override;

 private:
  std::shared_ptr<SpannerStub> child_;
  TracingOptions tracing_options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_LOGGING_SPANNER_STUB_H
