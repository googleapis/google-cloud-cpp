// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/internal/async_retry_multi_page.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc_and_poll.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/async_retry_unary_rpc.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/internal/time_utils.h"
#include <google/protobuf/duration.pb.h>
#include <sstream>

namespace btadmin = ::google::bigtable::admin::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
static_assert(std::is_copy_constructible<bigtable::TableAdmin>::value,
              "bigtable::TableAdmin must be constructible");
static_assert(std::is_copy_assignable<bigtable::TableAdmin>::value,
              "bigtable::TableAdmin must be assignable");

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::ENCRYPTION_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::FULL;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::NAME_ONLY;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::REPLICATION_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::SCHEMA_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::VIEW_UNSPECIFIED;

/// Shortcuts to avoid typing long names over and over.
using ClientUtils = bigtable::internal::UnaryClientUtils<AdminClient>;
using google::cloud::internal::Idempotency;

StatusOr<btadmin::Table> TableAdmin::CreateTable(std::string table_id,
                                                 TableConfig config) {
  grpc::Status status;

  auto request = std::move(config).as_proto();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));

  // This is a non-idempotent API, use the correct retry loop for this type of
  // operation.
  auto result = ClientUtils::MakeNonIdempotentCall(
      *client_, clone_rpc_retry_policy(), clone_metadata_update_policy(),
      &AdminClient::CreateTable, request, "CreateTable", status);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return result;
}

StatusOr<std::vector<btadmin::Table>> TableAdmin::ListTables(
    btadmin::Table::View view) {
  grpc::Status status;

  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  // Build the RPC request, try to minimize copying.
  std::vector<btadmin::Table> result;
  std::string page_token;
  do {
    btadmin::ListTablesRequest request;
    request.set_page_token(std::move(page_token));
    request.set_parent(instance_name());
    request.set_view(view);

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, clone_metadata_update_policy(),
        &AdminClient::ListTables, request, "TableAdmin", status,
        Idempotency::kIdempotent);

    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }

    for (auto& x : *response.mutable_tables()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (!page_token.empty());
  return result;
}

StatusOr<btadmin::Table> TableAdmin::GetTable(std::string const& table_id,
                                              btadmin::Table::View view) {
  grpc::Status status;
  btadmin::GetTableRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_view(view);

  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);

  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::GetTable, request, "GetTable",
      status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return result;
}

Status TableAdmin::DeleteTable(std::string const& table_id) {
  grpc::Status status;
  btadmin::DeleteTableRequest request;
  auto name = TableName(table_id);
  request.set_name(name);

  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);

  // This is a non-idempotent API, use the correct retry loop for this type of
  // operation.
  ClientUtils::MakeNonIdempotentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::DeleteTable, request, "DeleteTable", status);

  return google::cloud::MakeStatusFromRpcError(status);
}

google::bigtable::admin::v2::CreateBackupRequest
TableAdmin::CreateBackupParams::AsProto(std::string instance_name) const {
  google::bigtable::admin::v2::CreateBackupRequest proto;
  proto.set_parent(instance_name + "/clusters/" + cluster_id);
  proto.set_backup_id(backup_id);
  proto.mutable_backup()->set_source_table(std::move(instance_name) +
                                           "/tables/" + table_name);
  *proto.mutable_backup()->mutable_expire_time() =
      google::cloud::internal::ToProtoTimestamp(expire_time);
  return proto;
}

StatusOr<google::bigtable::admin::v2::Backup> TableAdmin::CreateBackup(
    CreateBackupParams const& params) {
  auto cq = background_threads_->cq();
  return AsyncCreateBackupImpl(cq, params).get();
}

future<StatusOr<google::bigtable::admin::v2::Backup>>
TableAdmin::AsyncCreateBackupImpl(CompletionQueue& cq,
                                  CreateBackupParams const& params) {
  auto request = params.AsProto(instance_name());
  MetadataUpdatePolicy metadata_update_policy(request.parent(),
                                              MetadataParamTypes::PARENT);
  auto client = client_;
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Backup>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      metadata_update_policy, client,
      [client](grpc::ClientContext* context,
               google::bigtable::admin::v2::CreateBackupRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncCreateBackup(context, request, cq);
      },
      std::move(request), cq);
}

StatusOr<google::bigtable::admin::v2::Backup> TableAdmin::GetBackup(
    std::string const& cluster_id, std::string const& backup_id) {
  grpc::Status status;
  btadmin::GetBackupRequest request;
  auto name = google::cloud::bigtable::BackupName(project(), instance_id(),
                                                  cluster_id, backup_id);
  request.set_name(name);

  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);

  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::GetBackup, request, "GetBackup",
      status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return result;
}

google::bigtable::admin::v2::UpdateBackupRequest
TableAdmin::UpdateBackupParams::AsProto(
    std::string const& instance_name) const {
  google::bigtable::admin::v2::UpdateBackupRequest proto;
  proto.mutable_backup()->set_name(instance_name + "/clusters/" + cluster_id +
                                   "/backups/" + backup_name);
  *proto.mutable_backup()->mutable_expire_time() =
      google::cloud::internal::ToProtoTimestamp(expire_time);
  proto.mutable_update_mask()->add_paths("expire_time");
  return proto;
}

StatusOr<google::bigtable::admin::v2::Backup> TableAdmin::UpdateBackup(
    UpdateBackupParams const& params) {
  grpc::Status status;
  btadmin::UpdateBackupRequest request = params.AsProto(instance_name());

  MetadataUpdatePolicy metadata_update_policy(request.backup().name(),
                                              MetadataParamTypes::BACKUP_NAME);

  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::UpdateBackup, request,
      "UpdateBackup", status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return result;
}

Status TableAdmin::DeleteBackup(
    google::bigtable::admin::v2::Backup const& backup) {
  grpc::Status status;
  btadmin::DeleteBackupRequest request;
  request.set_name(backup.name());
  MetadataUpdatePolicy metadata_update_policy(request.name(),
                                              MetadataParamTypes::NAME);
  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::DeleteBackup, request,
      "DeleteBackup", status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return {};
}

Status TableAdmin::DeleteBackup(std::string const& cluster_id,
                                std::string const& backup_id) {
  grpc::Status status;
  btadmin::DeleteBackupRequest request;
  request.set_name(google::cloud::bigtable::BackupName(project(), instance_id(),
                                                       cluster_id, backup_id));

  MetadataUpdatePolicy metadata_update_policy(request.name(),
                                              MetadataParamTypes::NAME);

  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::DeleteBackup, request,
      "DeleteBackup", status, Idempotency::kIdempotent);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return {};
}

google::bigtable::admin::v2::ListBackupsRequest
TableAdmin::ListBackupsParams::AsProto(std::string const& instance_name) const {
  google::bigtable::admin::v2::ListBackupsRequest proto;
  proto.set_parent(cluster_id ? instance_name + "/clusters/" + *cluster_id
                              : instance_name + "/clusters/-");
  if (filter) *proto.mutable_filter() = *filter;
  if (order_by) *proto.mutable_order_by() = *order_by;
  return proto;
}

StatusOr<std::vector<google::bigtable::admin::v2::Backup>>
TableAdmin::ListBackups(ListBackupsParams const& params) {
  grpc::Status status;

  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  // Build the RPC request, try to minimize copying.
  std::vector<btadmin::Backup> result;
  btadmin::ListBackupsRequest request = params.AsProto(instance_name());

  MetadataUpdatePolicy metadata_update_policy(request.parent(),
                                              MetadataParamTypes::PARENT);

  std::string page_token;
  do {
    request.set_page_token(std::move(page_token));

    auto response = ClientUtils::MakeCall(
        *client_, *rpc_policy, *backoff_policy, metadata_update_policy,
        &AdminClient::ListBackups, request, "TableAdmin", status,
        Idempotency::kIdempotent);

    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }

    for (auto& x : *response.mutable_backups()) {
      result.emplace_back(std::move(x));
    }
    page_token = std::move(*response.mutable_next_page_token());
  } while (!page_token.empty());
  return result;
}

google::bigtable::admin::v2::RestoreTableRequest
TableAdmin::RestoreTableParams::AsProto(
    std::string const& instance_name) const {
  google::bigtable::admin::v2::RestoreTableRequest proto;
  proto.set_parent(instance_name);
  proto.set_table_id(table_id);
  proto.set_backup(instance_name + "/clusters/" + cluster_id + "/backups/" +
                   backup_id);
  return proto;
}

StatusOr<google::bigtable::admin::v2::Table> TableAdmin::RestoreTable(
    RestoreTableParams const& params) {
  auto cq = background_threads_->cq();
  return AsyncRestoreTableImpl(cq, params).get();
}

future<StatusOr<google::bigtable::admin::v2::Table>>
TableAdmin::AsyncRestoreTableImpl(CompletionQueue& cq,
                                  RestoreTableParams const& params) {
  return AsyncRestoreTableImpl(
      cq,
      RestoreTableFromInstanceParams{
          params.table_id, BackupName(params.cluster_id, params.backup_id)});
}

google::bigtable::admin::v2::RestoreTableRequest AsProto(
    std::string const& instance_name,
    TableAdmin::RestoreTableFromInstanceParams p) {
  google::bigtable::admin::v2::RestoreTableRequest proto;
  proto.set_parent(instance_name);
  proto.set_table_id(std::move(p.table_id));
  proto.set_backup(std::move(p.backup_name));
  return proto;
}

StatusOr<google::bigtable::admin::v2::Table> TableAdmin::RestoreTable(
    RestoreTableFromInstanceParams params) {
  auto cq = background_threads_->cq();
  return AsyncRestoreTableImpl(cq, std::move(params)).get();
}

future<StatusOr<google::bigtable::admin::v2::Table>>
TableAdmin::AsyncRestoreTableImpl(CompletionQueue& cq,
                                  RestoreTableFromInstanceParams params) {
  MetadataUpdatePolicy metadata_update_policy(instance_name(),
                                              MetadataParamTypes::PARENT);
  auto client = client_;
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Table>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(),
      internal::ConstantIdempotencyPolicy(Idempotency::kNonIdempotent),
      metadata_update_policy, client,
      [client](grpc::ClientContext* context,
               google::bigtable::admin::v2::RestoreTableRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncRestoreTable(context, request, cq);
      },
      AsProto(instance_name(), std::move(params)), cq);
}

StatusOr<btadmin::Table> TableAdmin::ModifyColumnFamilies(
    std::string const& table_id,
    std::vector<ColumnFamilyModification> modifications) {
  grpc::Status status;

  btadmin::ModifyColumnFamiliesRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  for (auto& m : modifications) {
    *request.add_modifications() = std::move(m).as_proto();
  }
  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);
  auto result = ClientUtils::MakeNonIdempotentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::ModifyColumnFamilies, request, "ModifyColumnFamilies",
      status);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return result;
}

Status TableAdmin::DropRowsByPrefix(std::string const& table_id,
                                    std::string row_key_prefix) {
  grpc::Status status;
  btadmin::DropRowRangeRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_row_key_prefix(std::move(row_key_prefix));
  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);
  ClientUtils::MakeNonIdempotentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::DropRowRange, request, "DropRowByPrefix", status);

  return google::cloud::MakeStatusFromRpcError(status);
}

google::cloud::future<StatusOr<Consistency>> TableAdmin::WaitForConsistency(
    std::string const& table_id, std::string const& consistency_token) {
  auto cq = background_threads_->cq();
  return AsyncWaitForConsistencyImpl(cq, table_id, consistency_token);
}

google::cloud::future<StatusOr<Consistency>>
TableAdmin::AsyncWaitForConsistencyImpl(CompletionQueue& cq,
                                        std::string const& table_id,
                                        std::string const& consistency_token) {
  class AsyncWaitForConsistencyState
      : public std::enable_shared_from_this<AsyncWaitForConsistencyState> {
   public:
    static future<StatusOr<Consistency>> Create(
        CompletionQueue cq, std::string table_id, std::string consistency_token,
        TableAdmin const& table_admin,
        std::unique_ptr<PollingPolicy> polling_policy) {
      std::shared_ptr<AsyncWaitForConsistencyState> state(
          new AsyncWaitForConsistencyState(
              std::move(cq), std::move(table_id), std::move(consistency_token),
              table_admin, std::move(polling_policy)));

      state->StartIteration();
      return state->promise_.get_future();
    }

   private:
    AsyncWaitForConsistencyState(CompletionQueue cq, std::string table_id,
                                 std::string consistency_token,
                                 TableAdmin const& table_admin,
                                 std::unique_ptr<PollingPolicy> polling_policy)
        : cq_(std::move(cq)),
          table_id_(std::move(table_id)),
          consistency_token_(std::move(consistency_token)),
          table_admin_(table_admin),
          polling_policy_(std::move(polling_policy)) {}

    void StartIteration() {
      auto self = shared_from_this();
      table_admin_.AsyncCheckConsistency(cq_, table_id_, consistency_token_)
          .then([self](future<StatusOr<Consistency>> f) {
            self->OnCheckConsistency(f.get());
          });
    }

    void OnCheckConsistency(StatusOr<Consistency> consistent) {
      auto self = shared_from_this();
      if (consistent && *consistent == Consistency::kConsistent) {
        promise_.set_value(*consistent);
        return;
      }
      auto status = std::move(consistent).status();
      if (!polling_policy_->OnFailure(status)) {
        promise_.set_value(std::move(status));
        return;
      }
      cq_.MakeRelativeTimer(polling_policy_->WaitPeriod())
          .then([self](future<StatusOr<std::chrono::system_clock::time_point>>
                           result) {
            if (auto tp = result.get()) {
              self->StartIteration();
            } else {
              self->promise_.set_value(tp.status());
            }
          });
    }

    CompletionQueue cq_;
    std::string table_id_;
    std::string consistency_token_;
    TableAdmin table_admin_;
    std::unique_ptr<PollingPolicy> polling_policy_;
    google::cloud::promise<StatusOr<Consistency>> promise_;
  };

  return AsyncWaitForConsistencyState::Create(cq, table_id, consistency_token,
                                              *this, clone_polling_policy());
}

Status TableAdmin::DropAllRows(std::string const& table_id) {
  grpc::Status status;
  btadmin::DropRowRangeRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_delete_all_data_from_table(true);
  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);
  ClientUtils::MakeNonIdempotentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::DropRowRange, request, "DropAllRows", status);

  return google::cloud::MakeStatusFromRpcError(status);
}

StatusOr<std::string> TableAdmin::GenerateConsistencyToken(
    std::string const& table_id) {
  grpc::Status status;
  btadmin::GenerateConsistencyTokenRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);

  auto response = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::GenerateConsistencyToken, request,
      "GenerateConsistencyToken", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return std::move(*response.mutable_consistency_token());
}

StatusOr<Consistency> TableAdmin::CheckConsistency(
    std::string const& table_id, std::string const& consistency_token) {
  grpc::Status status;
  btadmin::CheckConsistencyRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_consistency_token(consistency_token);
  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);

  auto response = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::CheckConsistency, request,
      "CheckConsistency", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return response.consistent() ? Consistency::kConsistent
                               : Consistency::kInconsistent;
}

future<StatusOr<Consistency>> TableAdmin::AsyncCheckConsistency(
    CompletionQueue& cq, std::string const& table_id,
    std::string const& consistency_token) {
  btadmin::CheckConsistencyRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_consistency_token(consistency_token);

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             Idempotency::kIdempotent,
             [client, name](grpc::ClientContext* context,
                            btadmin::CheckConsistencyRequest const& request,
                            grpc::CompletionQueue* cq) {
               MetadataUpdatePolicy(name, MetadataParamTypes::NAME)
                   .Setup(*context);
               return client->AsyncCheckConsistency(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<btadmin::CheckConsistencyResponse>> fut)
                -> StatusOr<Consistency> {
        auto result = fut.get();
        if (!result) {
          return result.status();
        }

        return result->consistent() ? Consistency::kConsistent
                                    : Consistency::kInconsistent;
      });
}

StatusOr<google::iam::v1::Policy> TableAdmin::GetIamPolicy(
    std::string const& table_id) {
  grpc::Status status;

  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *client_, *clone_rpc_retry_policy(), *clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::GetIamPolicy, request,
      "GetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

StatusOr<google::iam::v1::Policy> TableAdmin::GetIamPolicy(
    std::string const& cluster_id, std::string const& backup_id) {
  grpc::Status status;

  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = BackupName(cluster_id, backup_id);
  request.set_resource(resource);

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *client_, *clone_rpc_retry_policy(), *clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::GetIamPolicy, request,
      "GetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

StatusOr<google::iam::v1::Policy> TableAdmin::SetIamPolicy(
    std::string const& table_id, google::iam::v1::Policy const& iam_policy) {
  grpc::Status status;

  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);
  *request.mutable_policy() = iam_policy;

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *client_, *clone_rpc_retry_policy(), *clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::SetIamPolicy, request,
      "SetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

StatusOr<google::iam::v1::Policy> TableAdmin::SetIamPolicy(
    std::string const& cluster_id, std::string const& backup_id,
    google::iam::v1::Policy const& iam_policy) {
  grpc::Status status;

  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = BackupName(cluster_id, backup_id);
  request.set_resource(resource);
  *request.mutable_policy() = iam_policy;

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *client_, *clone_rpc_retry_policy(), *clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::SetIamPolicy, request,
      "SetIamPolicy", status, Idempotency::kIdempotent);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

StatusOr<std::vector<std::string>> TableAdmin::TestIamPermissions(
    std::string const& table_id, std::vector<std::string> const& permissions) {
  grpc::Status status;
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);

  for (auto const& permission : permissions) {
    request.add_permissions(permission);
  }

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto response = ClientUtils::MakeCall(
      *client_, *clone_rpc_retry_policy(), *clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::TestIamPermissions, request,
      "TestIamPermissions", status, Idempotency::kIdempotent);

  std::vector<std::string> resource_permissions;

  for (auto& permission : *response.mutable_permissions()) {
    resource_permissions.push_back(permission);
  }

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return resource_permissions;
}

StatusOr<std::vector<std::string>> TableAdmin::TestIamPermissions(
    std::string const& cluster_id, std::string const& backup_id,
    std::vector<std::string> const& permissions) {
  grpc::Status status;
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto resource = BackupName(cluster_id, backup_id);
  request.set_resource(resource);

  for (auto const& permission : permissions) {
    request.add_permissions(permission);
  }

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto response = ClientUtils::MakeCall(
      *client_, *clone_rpc_retry_policy(), *clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::TestIamPermissions, request,
      "TestIamPermissions", status, Idempotency::kIdempotent);

  std::vector<std::string> resource_permissions;

  for (auto& permission : *response.mutable_permissions()) {
    resource_permissions.push_back(permission);
  }

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return resource_permissions;
}

std::string TableAdmin::InstanceName() const {
  return google::cloud::bigtable::InstanceName(client_->project(),
                                               instance_id_);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
