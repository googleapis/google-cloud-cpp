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

#include "google/cloud/spanner/internal/logging_spanner_stub.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

template <typename T>
struct IsStatusOr : public std::false_type {};
template <typename T>
struct IsStatusOr<StatusOr<T>> : public std::true_type {};

template <typename T>
struct IsUniquePtr : public std::false_type {};
template <typename T>
struct IsUniquePtr<std::unique_ptr<T>> : public std::true_type {};

template <
    typename Functor, typename Request,
    typename Result = google::cloud::internal::invoke_result_t<
        Functor, grpc::ClientContext&, Request const&>,
    typename std::enable_if<std::is_same<Result, google::cloud::Status>::value,
                            int>::type = 0>
Result LogHelper(Functor&& functor, grpc::ClientContext& context,
                 Request const& request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> status=" << response;
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext&, Request const&>,
          typename std::enable_if<IsStatusOr<Result>::value, int>::type = 0>
Result LogHelper(Functor&& functor, grpc::ClientContext& context,
                 Request const& request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(context, request);
  if (!response) {
    GCP_LOG(DEBUG) << where << "() >> status=" << response.status();
  } else {
    GCP_LOG(DEBUG) << where << "() >> response=" << response->DebugString();
  }
  return response;
}

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, grpc::ClientContext&, Request const&>,
          typename std::enable_if<IsUniquePtr<Result>::value, int>::type = 0>
Result LogHelper(Functor&& functor, grpc::ClientContext& context,
                 Request const& request, char const* where) {
  GCP_LOG(DEBUG) << where << "() << " << request.DebugString();
  auto response = functor(context, request);
  GCP_LOG(DEBUG) << where << "() >> " << (response ? "null" : "not null")
                 << " stream";
  return response;
}

}  // namespace

namespace spanner_proto = ::google::spanner::v1;

StatusOr<spanner_proto::Session> LoggingSpannerStub::CreateSession(
    grpc::ClientContext& client_context,
    spanner_proto::CreateSessionRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::CreateSessionRequest const& request) {
        return child_->CreateSession(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::Session> LoggingSpannerStub::GetSession(
    grpc::ClientContext& client_context,
    spanner_proto::GetSessionRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::GetSessionRequest const& request) {
        return child_->GetSession(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::ListSessionsResponse> LoggingSpannerStub::ListSessions(
    grpc::ClientContext& client_context,
    spanner_proto::ListSessionsRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::ListSessionsRequest const& request) {
        return child_->ListSessions(context, request);
      },
      client_context, request, __func__);
}

Status LoggingSpannerStub::DeleteSession(
    grpc::ClientContext& client_context,
    spanner_proto::DeleteSessionRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::DeleteSessionRequest const& request) {
        return child_->DeleteSession(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::ResultSet> LoggingSpannerStub::ExecuteSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::ExecuteSqlRequest const& request) {
        return child_->ExecuteSql(context, request);
      },
      client_context, request, __func__);
}

std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
LoggingSpannerStub::ExecuteStreamingSql(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteSqlRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::ExecuteSqlRequest const& request) {
        return child_->ExecuteStreamingSql(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::ExecuteBatchDmlResponse>
LoggingSpannerStub::ExecuteBatchDml(
    grpc::ClientContext& client_context,
    spanner_proto::ExecuteBatchDmlRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::ExecuteBatchDmlRequest const& request) {
        return child_->ExecuteBatchDml(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::ResultSet> LoggingSpannerStub::Read(
    grpc::ClientContext& client_context,
    spanner_proto::ReadRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::ReadRequest const& request) {
        return child_->Read(context, request);
      },
      client_context, request, __func__);
}

std::unique_ptr<grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>
LoggingSpannerStub::StreamingRead(grpc::ClientContext& client_context,
                                  spanner_proto::ReadRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::ReadRequest const& request) {
        return child_->StreamingRead(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::Transaction> LoggingSpannerStub::BeginTransaction(
    grpc::ClientContext& client_context,
    spanner_proto::BeginTransactionRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::BeginTransactionRequest const& request) {
        return child_->BeginTransaction(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::CommitResponse> LoggingSpannerStub::Commit(
    grpc::ClientContext& client_context,
    spanner_proto::CommitRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::CommitRequest const& request) {
        return child_->Commit(context, request);
      },
      client_context, request, __func__);
}

Status LoggingSpannerStub::Rollback(
    grpc::ClientContext& client_context,
    spanner_proto::RollbackRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::RollbackRequest const& request) {
        return child_->Rollback(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::PartitionResponse> LoggingSpannerStub::PartitionQuery(
    grpc::ClientContext& client_context,
    spanner_proto::PartitionQueryRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::PartitionQueryRequest const& request) {
        return child_->PartitionQuery(context, request);
      },
      client_context, request, __func__);
}

StatusOr<spanner_proto::PartitionResponse> LoggingSpannerStub::PartitionRead(
    grpc::ClientContext& client_context,
    spanner_proto::PartitionReadRequest const& request) {
  return LogHelper(
      [this](grpc::ClientContext& context,
             spanner_proto::PartitionReadRequest const& request) {
        return child_->PartitionRead(context, request);
      },
      client_context, request, __func__);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
