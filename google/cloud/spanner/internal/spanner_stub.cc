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

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/internal/logging_spanner_stub.h"
#include "google/cloud/spanner/internal/metadata_spanner_stub.h"
#include "google/cloud/spanner/internal/spanner_auth.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/log.h"
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/**
 * DefaultSpannerStub - a stub that calls Spanner's gRPC interface.
 */
class DefaultSpannerStub : public SpannerStub {
 public:
  explicit DefaultSpannerStub(
      std::unique_ptr<google::spanner::v1::Spanner::StubInterface> grpc_stub)
      : grpc_stub_(std::move(grpc_stub)) {}

  DefaultSpannerStub(DefaultSpannerStub const&) = delete;
  DefaultSpannerStub& operator=(DefaultSpannerStub const&) = delete;

  StatusOr<google::spanner::v1::Session> CreateSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::CreateSessionRequest const& request) override;
  StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
  BatchCreateSessions(
      grpc::ClientContext& client_context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) override;
  future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::spanner::v1::BatchCreateSessionsRequest const& request) override;
  Status DeleteSession(
      grpc::ClientContext& client_context,
      google::spanner::v1::DeleteSessionRequest const& request) override;
  future<Status> AsyncDeleteSession(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::spanner::v1::DeleteSessionRequest const& request) override;
  StatusOr<google::spanner::v1::ResultSet> ExecuteSql(
      grpc::ClientContext& client_context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;
  future<StatusOr<google::spanner::v1::ResultSet>> AsyncExecuteSql(
      google::cloud::CompletionQueue& cq,
      std::unique_ptr<grpc::ClientContext> context,
      google::spanner::v1::ExecuteSqlRequest const& request) override;
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
  std::unique_ptr<google::spanner::v1::Spanner::StubInterface> grpc_stub_;
};

StatusOr<google::spanner::v1::Session> DefaultSpannerStub::CreateSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::CreateSessionRequest const& request) {
  google::spanner::v1::Session response;
  grpc::Status grpc_status =
      grpc_stub_->CreateSession(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<google::spanner::v1::BatchCreateSessionsResponse>
DefaultSpannerStub::BatchCreateSessions(
    grpc::ClientContext& client_context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  google::spanner::v1::BatchCreateSessionsResponse response;
  grpc::Status grpc_status =
      grpc_stub_->BatchCreateSessions(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>
DefaultSpannerStub::AsyncBatchCreateSessions(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::BatchCreateSessionsRequest const& request) {
  return cq.MakeUnaryRpc(
      [this](grpc::ClientContext* context,
             google::spanner::v1::BatchCreateSessionsRequest const& request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncBatchCreateSessions(context, request, cq);
      },
      request, std::move(context));
}

Status DefaultSpannerStub::DeleteSession(
    grpc::ClientContext& client_context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  google::protobuf::Empty response;
  grpc::Status grpc_status =
      grpc_stub_->DeleteSession(&client_context, request, &response);
  return google::cloud::MakeStatusFromRpcError(grpc_status);
}

future<Status> DefaultSpannerStub::AsyncDeleteSession(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::DeleteSessionRequest const& request) {
  return cq
      .MakeUnaryRpc(
          [this](grpc::ClientContext* context,
                 google::spanner::v1::DeleteSessionRequest const& request,
                 grpc::CompletionQueue* cq) {
            return grpc_stub_->AsyncDeleteSession(context, request, cq);
          },
          request, std::move(context))
      .then([](future<StatusOr<google::protobuf::Empty>> f) {
        return f.get().status();
      });
}

StatusOr<google::spanner::v1::ResultSet> DefaultSpannerStub::ExecuteSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  google::spanner::v1::ResultSet response;
  grpc::Status grpc_status =
      grpc_stub_->ExecuteSql(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

future<StatusOr<google::spanner::v1::ResultSet>>
DefaultSpannerStub::AsyncExecuteSql(
    google::cloud::CompletionQueue& cq,
    std::unique_ptr<grpc::ClientContext> context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  return cq.MakeUnaryRpc(
      [this](grpc::ClientContext* context,
             google::spanner::v1::ExecuteSqlRequest const& request,
             grpc::CompletionQueue* cq) {
        return grpc_stub_->AsyncExecuteSql(context, request, cq);
      },
      request, std::move(context));
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
DefaultSpannerStub::ExecuteStreamingSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request) {
  return grpc_stub_->ExecuteStreamingSql(&client_context, request);
}

StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>
DefaultSpannerStub::ExecuteBatchDml(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteBatchDmlRequest const& request) {
  google::spanner::v1::ExecuteBatchDmlResponse response;
  grpc::Status grpc_status =
      grpc_stub_->ExecuteBatchDml(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

std::unique_ptr<
    grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
DefaultSpannerStub::StreamingRead(
    grpc::ClientContext& client_context,
    google::spanner::v1::ReadRequest const& request) {
  return grpc_stub_->StreamingRead(&client_context, request);
}

StatusOr<google::spanner::v1::Transaction> DefaultSpannerStub::BeginTransaction(
    grpc::ClientContext& client_context,
    google::spanner::v1::BeginTransactionRequest const& request) {
  google::spanner::v1::Transaction response;
  grpc::Status grpc_status =
      grpc_stub_->BeginTransaction(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<google::spanner::v1::CommitResponse> DefaultSpannerStub::Commit(
    grpc::ClientContext& client_context,
    google::spanner::v1::CommitRequest const& request) {
  google::spanner::v1::CommitResponse response;
  grpc::Status grpc_status =
      grpc_stub_->Commit(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

Status DefaultSpannerStub::Rollback(
    grpc::ClientContext& client_context,
    google::spanner::v1::RollbackRequest const& request) {
  google::protobuf::Empty response;
  grpc::Status grpc_status =
      grpc_stub_->Rollback(&client_context, request, &response);
  return google::cloud::MakeStatusFromRpcError(grpc_status);
}

StatusOr<google::spanner::v1::PartitionResponse>
DefaultSpannerStub::PartitionQuery(
    grpc::ClientContext& client_context,
    google::spanner::v1::PartitionQueryRequest const& request) {
  google::spanner::v1::PartitionResponse response;
  grpc::Status grpc_status =
      grpc_stub_->PartitionQuery(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<google::spanner::v1::PartitionResponse>
DefaultSpannerStub::PartitionRead(
    grpc::ClientContext& client_context,
    google::spanner::v1::PartitionReadRequest const& request) {
  google::spanner::v1::PartitionResponse response;
  grpc::Status grpc_status =
      grpc_stub_->PartitionRead(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

}  // namespace

std::shared_ptr<SpannerStub> CreateDefaultSpannerStub(
    spanner::Database const& db,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    Options const& opts, int channel_id) {
  grpc::ChannelArguments channel_arguments =
      internal::MakeChannelArguments(opts);
  // Newer versions of gRPC include a macro (`GRPC_ARG_CHANNEL_ID`) but use
  // its value here to allow compiling against older versions.
  channel_arguments.SetInt("grpc.channel_id", channel_id);

  auto channel =
      auth->CreateChannel(opts.get<EndpointOption>(), channel_arguments);
  auto spanner_grpc_stub = google::spanner::v1::Spanner::NewStub(channel);
  std::shared_ptr<SpannerStub> stub =
      std::make_shared<DefaultSpannerStub>(std::move(spanner_grpc_stub));

  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<SpannerAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<MetadataSpannerStub>(std::move(stub), db.FullName());
  if (internal::Contains(opts.get<TracingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<LoggingSpannerStub>(
        std::move(stub), opts.get<GrpcTracingOptionsOption>());
  }
  return stub;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
