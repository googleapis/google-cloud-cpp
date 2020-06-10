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

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/internal/logging_spanner_stub.h"
#include "google/cloud/spanner/internal/metadata_spanner_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/log.h"
#include <google/spanner/v1/spanner.grpc.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

namespace spanner_proto = ::google::spanner::v1;

/**
 * DefaultSpannerStub - a stub that calls Spanner's gRPC interface.
 */
class DefaultSpannerStub : public SpannerStub {
 public:
  explicit DefaultSpannerStub(
      std::unique_ptr<spanner_proto::Spanner::StubInterface> grpc_stub)
      : grpc_stub_(std::move(grpc_stub)) {}

  DefaultSpannerStub(DefaultSpannerStub const&) = delete;
  DefaultSpannerStub& operator=(DefaultSpannerStub const&) = delete;

  StatusOr<spanner_proto::Session> CreateSession(
      grpc::ClientContext& client_context,
      spanner_proto::CreateSessionRequest const& request) override;
  StatusOr<spanner_proto::BatchCreateSessionsResponse> BatchCreateSessions(
      grpc::ClientContext& client_context,
      spanner_proto::BatchCreateSessionsRequest const& request) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      spanner_proto::BatchCreateSessionsResponse>>
  AsyncBatchCreateSessions(
      grpc::ClientContext& client_context,
      spanner_proto::BatchCreateSessionsRequest const& request,
      grpc::CompletionQueue* cq) override;
  StatusOr<spanner_proto::Session> GetSession(
      grpc::ClientContext& client_context,
      spanner_proto::GetSessionRequest const& request) override;
  StatusOr<spanner_proto::ListSessionsResponse> ListSessions(
      grpc::ClientContext& client_context,
      spanner_proto::ListSessionsRequest const& request) override;
  Status DeleteSession(
      grpc::ClientContext& client_context,
      spanner_proto::DeleteSessionRequest const& request) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteSession(grpc::ClientContext& client_context,
                     spanner_proto::DeleteSessionRequest const& request,
                     grpc::CompletionQueue* cq) override;
  StatusOr<spanner_proto::ResultSet> ExecuteSql(
      grpc::ClientContext& client_context,
      spanner_proto::ExecuteSqlRequest const& request) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::spanner::v1::ResultSet>>
  AsyncExecuteSql(grpc::ClientContext& client_context,
                  google::spanner::v1::ExecuteSqlRequest const& request,
                  grpc::CompletionQueue* cq) override;
  std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
  ExecuteStreamingSql(grpc::ClientContext& client_context,
                      spanner_proto::ExecuteSqlRequest const& request) override;
  StatusOr<spanner_proto::ExecuteBatchDmlResponse> ExecuteBatchDml(
      grpc::ClientContext& client_context,
      spanner_proto::ExecuteBatchDmlRequest const& request) override;
  std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
  StreamingRead(grpc::ClientContext& client_context,
                spanner_proto::ReadRequest const& request) override;
  StatusOr<spanner_proto::Transaction> BeginTransaction(
      grpc::ClientContext& client_context,
      spanner_proto::BeginTransactionRequest const& request) override;
  StatusOr<spanner_proto::CommitResponse> Commit(
      grpc::ClientContext& client_context,
      spanner_proto::CommitRequest const& request) override;
  Status Rollback(grpc::ClientContext& client_context,
                  spanner_proto::RollbackRequest const& request) override;
  StatusOr<spanner_proto::PartitionResponse> PartitionQuery(
      grpc::ClientContext& client_context,
      spanner_proto::PartitionQueryRequest const& request) override;
  StatusOr<spanner_proto::PartitionResponse> PartitionRead(
      grpc::ClientContext& client_context,
      spanner_proto::PartitionReadRequest const& request) override;

 private:
  std::unique_ptr<spanner_proto::Spanner::StubInterface> grpc_stub_;
};

StatusOr<spanner_proto::Session> DefaultSpannerStub::CreateSession(
    grpc::ClientContext& client_context,
    spanner_proto::CreateSessionRequest const& request) {
  spanner_proto::Session response;
  grpc::Status grpc_status =
      grpc_stub_->CreateSession(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<spanner_proto::BatchCreateSessionsResponse>
DefaultSpannerStub::BatchCreateSessions(
    grpc::ClientContext& client_context,
    spanner_proto::BatchCreateSessionsRequest const& request) {
  spanner_proto::BatchCreateSessionsResponse response;
  grpc::Status grpc_status =
      grpc_stub_->BatchCreateSessions(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    spanner_proto::BatchCreateSessionsResponse>>
DefaultSpannerStub::AsyncBatchCreateSessions(
    grpc::ClientContext& client_context,
    spanner_proto::BatchCreateSessionsRequest const& request,
    grpc::CompletionQueue* cq) {
  return grpc_stub_->AsyncBatchCreateSessions(&client_context, request, cq);
}

StatusOr<spanner_proto::Session> DefaultSpannerStub::GetSession(
    grpc::ClientContext& client_context,
    spanner_proto::GetSessionRequest const& request) {
  spanner_proto::Session response;
  grpc::Status grpc_status =
      grpc_stub_->GetSession(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<spanner_proto::ListSessionsResponse> DefaultSpannerStub::ListSessions(
    grpc::ClientContext& client_context,
    spanner_proto::ListSessionsRequest const& request) {
  spanner_proto::ListSessionsResponse response;
  grpc::Status grpc_status =
      grpc_stub_->ListSessions(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

Status DefaultSpannerStub::DeleteSession(
    grpc::ClientContext& client_context,
    spanner_proto::DeleteSessionRequest const& request) {
  google::protobuf::Empty response;
  grpc::Status grpc_status =
      grpc_stub_->DeleteSession(&client_context, request, &response);
  return google::cloud::MakeStatusFromRpcError(grpc_status);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
DefaultSpannerStub::AsyncDeleteSession(
    grpc::ClientContext& client_context,
    spanner_proto::DeleteSessionRequest const& request,
    grpc::CompletionQueue* cq) {
  return grpc_stub_->AsyncDeleteSession(&client_context, request, cq);
}

StatusOr<spanner_proto::ResultSet> DefaultSpannerStub::ExecuteSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request) {
  spanner_proto::ResultSet response;
  grpc::Status grpc_status =
      grpc_stub_->ExecuteSql(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<spanner_proto::ResultSet>>
DefaultSpannerStub::AsyncExecuteSql(
    grpc::ClientContext& client_context,
    google::spanner::v1::ExecuteSqlRequest const& request,
    grpc::CompletionQueue* cq) {
  return grpc_stub_->AsyncExecuteSql(&client_context, request, cq);
}

std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
DefaultSpannerStub::ExecuteStreamingSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request) {
  return grpc_stub_->ExecuteStreamingSql(&client_context, request);
}

StatusOr<spanner_proto::ExecuteBatchDmlResponse>
DefaultSpannerStub::ExecuteBatchDml(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteBatchDmlRequest const& request) {
  spanner_proto::ExecuteBatchDmlResponse response;
  grpc::Status grpc_status =
      grpc_stub_->ExecuteBatchDml(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
DefaultSpannerStub::StreamingRead(grpc::ClientContext& client_context,
                                  spanner_proto::ReadRequest const& request) {
  return grpc_stub_->StreamingRead(&client_context, request);
}

StatusOr<spanner_proto::Transaction> DefaultSpannerStub::BeginTransaction(
    grpc::ClientContext& client_context,
    spanner_proto::BeginTransactionRequest const& request) {
  spanner_proto::Transaction response;
  grpc::Status grpc_status =
      grpc_stub_->BeginTransaction(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<spanner_proto::CommitResponse> DefaultSpannerStub::Commit(
    grpc::ClientContext& client_context,
    spanner_proto::CommitRequest const& request) {
  spanner_proto::CommitResponse response;
  grpc::Status grpc_status =
      grpc_stub_->Commit(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

Status DefaultSpannerStub::Rollback(
    grpc::ClientContext& client_context,
    spanner_proto::RollbackRequest const& request) {
  google::protobuf::Empty response;
  grpc::Status grpc_status =
      grpc_stub_->Rollback(&client_context, request, &response);
  return google::cloud::MakeStatusFromRpcError(grpc_status);
}

StatusOr<spanner_proto::PartitionResponse> DefaultSpannerStub::PartitionQuery(
    grpc::ClientContext& client_context,
    spanner_proto::PartitionQueryRequest const& request) {
  spanner_proto::PartitionResponse response;
  grpc::Status grpc_status =
      grpc_stub_->PartitionQuery(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

StatusOr<spanner_proto::PartitionResponse> DefaultSpannerStub::PartitionRead(
    grpc::ClientContext& client_context,
    spanner_proto::PartitionReadRequest const& request) {
  spanner_proto::PartitionResponse response;
  grpc::Status grpc_status =
      grpc_stub_->PartitionRead(&client_context, request, &response);
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return response;
}

}  // namespace

std::shared_ptr<SpannerStub> CreateDefaultSpannerStub(ConnectionOptions options,
                                                      int channel_id) {
  options = internal::EmulatorOverrides(std::move(options));

  grpc::ChannelArguments channel_arguments = options.CreateChannelArguments();
  // Newer versions of gRPC include a macro (`GRPC_ARG_CHANNEL_ID`) but use
  // its value here to allow compiling against older versions.
  channel_arguments.SetInt("grpc.channel_id", channel_id);

  auto spanner_grpc_stub =
      spanner_proto::Spanner::NewStub(grpc::CreateCustomChannel(
          options.endpoint(), options.credentials(), channel_arguments));

  std::shared_ptr<SpannerStub> stub =
      std::make_shared<DefaultSpannerStub>(std::move(spanner_grpc_stub));
  stub = std::make_shared<MetadataSpannerStub>(std::move(stub));

  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    return std::make_shared<LoggingSpannerStub>(std::move(stub),
                                                options.tracing_options());
  }
  return stub;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
