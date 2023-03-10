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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_STUB_H

#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * SpannerStub is a thin stub layer over the Cloud Spanner API to avoid
 * exposing the underlying transport stub (gRPC, etc.) directly.
 *
 * The API is defined in:
 * https://github.com/googleapis/googleapis/blob/master/google/spanner/v1/spanner.proto
 *
 * @note We don't use the GetSession(), ListSessions(), or Read() RPCs.
 */
class SpannerStub {
 public:
  virtual ~SpannerStub() = default;

  /// Not copyable or moveable.
  SpannerStub(SpannerStub const&) = delete;
  SpannerStub(SpannerStub&&) = delete;
  SpannerStub& operator=(SpannerStub const&) = delete;
  SpannerStub& operator=(SpannerStub&&) = delete;

  virtual StatusOr<google::spanner::v1::Session> CreateSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::CreateSessionRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
  BatchCreateSessions(
      grpc::ClientContext& client_context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) = 0;
  virtual future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) = 0;
  virtual Status DeleteSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::DeleteSessionRequest const& request) = 0;
  virtual future<Status> AsyncDeleteSession(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::spanner::v1::DeleteSessionRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::ResultSet> ExecuteSql(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteSqlRequest const& request) = 0;
  virtual future<StatusOr<google::spanner::v1::ResultSet>> AsyncExecuteSql(
      google::cloud::CompletionQueue& cq,
      std::shared_ptr<grpc::ClientContext> context,
      google::spanner::v1::ExecuteSqlRequest const& request) = 0;
  virtual std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
  ExecuteStreamingSql(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteSqlRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>
  ExecuteBatchDml(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteBatchDmlRequest const& request) = 0;
  virtual std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
  StreamingRead(grpc::ClientContext& client_context,
                google::spanner::v1::ReadRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::Transaction> BeginTransaction(
      grpc::ClientContext& client_context,
      google::spanner::v1::BeginTransactionRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::CommitResponse> Commit(
      grpc::ClientContext& client_context,
      google::spanner::v1::CommitRequest const& request) = 0;
  virtual Status Rollback(
      grpc::ClientContext& client_context,
      google::spanner::v1::RollbackRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::PartitionResponse> PartitionQuery(
      grpc::ClientContext& client_context,
      google::spanner::v1::PartitionQueryRequest const& request) = 0;
  virtual StatusOr<google::spanner::v1::PartitionResponse> PartitionRead(
      grpc::ClientContext& client_context,
      google::spanner::v1::PartitionReadRequest const& request) = 0;

 protected:
  SpannerStub() = default;
};

/**
 * Creates a SpannerStub configured with @p opts and @p channel_id.
 *
 * @p channel_id should be unique among all stubs in the same Connection pool,
 * to ensure they use different underlying connections.
 */
std::shared_ptr<SpannerStub> CreateDefaultSpannerStub(
    spanner::Database const& db,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    Options const& opts, int channel_id);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_STUB_H
