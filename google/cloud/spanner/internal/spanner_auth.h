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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_AUTH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_AUTH_H

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class SpannerAuth : public SpannerStub {
 public:
  SpannerAuth(std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
              std::shared_ptr<SpannerStub> child);
  ~SpannerAuth() override = default;

  StatusOr<google::spanner::v1::Session> CreateSession(
      grpc::ClientContext& context,
      google::spanner::v1::CreateSessionRequest const& request) override;

  StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
  BatchCreateSessions(
      grpc::ClientContext& context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) override;

  Status DeleteSession(
      grpc::ClientContext& context,
      google::spanner::v1::DeleteSessionRequest const& request) override;

  StatusOr<google::spanner::v1::ResultSet> ExecuteSql(
      grpc::ClientContext& context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;

  std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
  ExecuteStreamingSql(
      grpc::ClientContext& context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;

  StatusOr<google::spanner::v1::ExecuteBatchDmlResponse> ExecuteBatchDml(
      grpc::ClientContext& context,
      google::spanner::v1::ExecuteBatchDmlRequest const& request) override;

  std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
  StreamingRead(grpc::ClientContext& context,
                google::spanner::v1::ReadRequest const& request) override;

  StatusOr<google::spanner::v1::Transaction> BeginTransaction(
      grpc::ClientContext& context,
      google::spanner::v1::BeginTransactionRequest const& request) override;

  StatusOr<google::spanner::v1::CommitResponse> Commit(
      grpc::ClientContext& context,
      google::spanner::v1::CommitRequest const& request) override;

  Status Rollback(grpc::ClientContext& context,
                  google::spanner::v1::RollbackRequest const& request) override;

  StatusOr<google::spanner::v1::PartitionResponse> PartitionQuery(
      grpc::ClientContext& context,
      google::spanner::v1::PartitionQueryRequest const& request) override;

  StatusOr<google::spanner::v1::PartitionResponse> PartitionRead(
      grpc::ClientContext& context,
      google::spanner::v1::PartitionReadRequest const& request) override;

  future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) override;

  future<Status> AsyncDeleteSession(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      google::spanner::v1::DeleteSessionRequest const& request) override;

  future<StatusOr<google::spanner::v1::ResultSet>> AsyncExecuteSql(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;

 private:
  std::shared_ptr<internal::GrpcAuthenticationStrategy> auth_;
  std::shared_ptr<SpannerStub> child_;
};

/**
 * A `ClientReaderInterface<PartialResultSet>` returning a fixed error.
 *
 * This is used when the library cannot even start the streaming RPC (for
 * example, because setting up the credentials for the call failed) and we
 * want to represent the error as part of the stream.
 */
class ClientReaderInterfaceError : public grpc::ClientReaderInterface<
                                       google::spanner::v1::PartialResultSet> {
 public:
  // Note that `StatusCode` uses `grpc::StatusCode`-compatible values.
  explicit ClientReaderInterfaceError(Status const& status)
      : status_(static_cast<grpc::StatusCode>(status.code()),
                status.message()) {}

  bool Read(google::spanner::v1::PartialResultSet*) override { return false; }
  bool NextMessageSize(std::uint32_t*) override { return false; }
  grpc::Status Finish() override { return status_; }
  void WaitForInitialMetadata() override {}

 private:
  grpc::Status status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_AUTH_H
