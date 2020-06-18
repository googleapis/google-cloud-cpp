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
constexpr TableAdmin::TableView TableAdmin::VIEW_UNSPECIFIED;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::NAME_ONLY;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::SCHEMA_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::REPLICATION_VIEW;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr TableAdmin::TableView TableAdmin::FULL;

/// Shortcuts to avoid typing long names over and over.
using ClientUtils = bigtable::internal::UnaryClientUtils<AdminClient>;

StatusOr<btadmin::Table> TableAdmin::CreateTable(std::string table_id,
                                                 TableConfig config) {
  grpc::Status status;

  auto request = std::move(config).as_proto();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));

  // This is a non-idempotent API, use the correct retry loop for this type of
  // operation.
  auto result = ClientUtils::MakeNonIdemponentCall(
      *client_, clone_rpc_retry_policy(), clone_metadata_update_policy(),
      &AdminClient::CreateTable, request, "CreateTable", status);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<btadmin::Table>> TableAdmin::AsyncCreateTable(
    CompletionQueue& cq, std::string table_id, TableConfig config) {
  btadmin::CreateTableRequest request = std::move(config).as_proto();
  request.set_parent(instance_name());
  request.set_table_id(std::move(table_id));

  auto client = client_;
  auto metadata_update_policy = clone_metadata_update_policy();
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/false,
      [client, metadata_update_policy](
          grpc::ClientContext* context,
          btadmin::CreateTableRequest const& request,
          grpc::CompletionQueue* cq) {
        metadata_update_policy.Setup(*context);
        return client->AsyncCreateTable(context, request, cq);
      },
      std::move(request));
}

future<StatusOr<google::bigtable::admin::v2::Table>> TableAdmin::AsyncGetTable(
    CompletionQueue& cq, std::string const& table_id,
    btadmin::Table::View view) {
  google::bigtable::admin::v2::GetTableRequest request{};
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_view(view);

  // Copy the client because we lack C++14 extended lambda captures.
  auto client = client_;
  auto metadata_update_policy = clone_metadata_update_policy();
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/true,
      [client, metadata_update_policy](
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetTableRequest const& request,
          grpc::CompletionQueue* cq) {
        metadata_update_policy.Setup(*context);
        return client->AsyncGetTable(context, request, cq);
      },
      std::move(request));
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
        &AdminClient::ListTables, request, "TableAdmin", status, true);

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

future<StatusOr<std::vector<btadmin::Table>>> TableAdmin::AsyncListTables(
    CompletionQueue& cq, btadmin::Table::View view) {
  auto client = client_;
  btadmin::ListTablesRequest request;
  request.set_parent(instance_name());
  request.set_view(view);

  return internal::StartAsyncRetryMultiPage(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      clone_metadata_update_policy(),
      [client](grpc::ClientContext* context,
               btadmin::ListTablesRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncListTables(context, request, cq);
      },
      std::move(request), std::vector<btadmin::Table>(),
      [](std::vector<btadmin::Table> acc,
         btadmin::ListTablesResponse const& response) {
        std::move(response.tables().begin(), response.tables().end(),
                  std::back_inserter(acc));
        return acc;
      },
      cq);
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
      status, true);
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
  ClientUtils::MakeNonIdemponentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::DeleteTable, request, "DeleteTable", status);

  return google::cloud::MakeStatusFromRpcError(status);
}

future<Status> TableAdmin::AsyncDeleteTable(CompletionQueue& cq,
                                            std::string const& table_id) {
  grpc::Status status;
  btadmin::DeleteTableRequest request;
  auto name = TableName(table_id);
  request.set_name(name);

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, name](
                 grpc::ClientContext* context,
                 google::bigtable::admin::v2::DeleteTableRequest const& request,
                 grpc::CompletionQueue* cq) {
               MetadataUpdatePolicy(name, MetadataParamTypes::NAME)
                   .Setup(*context);
               return client->AsyncDeleteTable(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<google::protobuf::Empty>> r) {
        return r.get().status();
      });
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
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();
  return AsyncCreateBackup(cq, params)
      .then(
          [cq](
              future<StatusOr<google::bigtable::admin::v2::Backup>> f) mutable {
            cq.Shutdown();
            return f.get();
          })
      .get();
}

future<StatusOr<google::bigtable::admin::v2::Backup>>
TableAdmin::AsyncCreateBackup(CompletionQueue& cq,
                              CreateBackupParams const& params) {
  auto request = params.AsProto(instance_name());
  MetadataUpdatePolicy metadata_update_policy(request.parent(),
                                              MetadataParamTypes::PARENT);
  auto client = client_;
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Backup>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
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
  std::string name =
      instance_name() + "/clusters/" + cluster_id + "/backups/" + backup_id;
  request.set_name(name);

  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);

  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::GetBackup, request, "GetBackup",
      status, true);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return result;
}

future<StatusOr<google::bigtable::admin::v2::Backup>>
TableAdmin::AsyncGetBackup(CompletionQueue& cq, std::string const& cluster_id,
                           std::string const& backup_id) {
  google::bigtable::admin::v2::GetBackupRequest request{};
  std::string name =
      instance_name() + "/clusters/" + cluster_id + "/backups/" + backup_id;
  request.set_name(name);

  // Copy the client because we lack C++14 extended lambda captures.
  auto client = client_;
  auto metadata_update_policy = clone_metadata_update_policy();
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/true,
      [client, metadata_update_policy](
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GetBackupRequest const& request,
          grpc::CompletionQueue* cq) {
        metadata_update_policy.Setup(*context);
        return client->AsyncGetBackup(context, request, cq);
      },
      std::move(request));
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
      "UpdateBackup", status, true);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return result;
}

future<StatusOr<google::bigtable::admin::v2::Backup>>
TableAdmin::AsyncUpdateBackup(CompletionQueue& cq,
                              UpdateBackupParams const& params) {
  btadmin::UpdateBackupRequest request = params.AsProto(instance_name());

  // Copy the client because we lack C++14 extended lambda captures.
  auto client = client_;
  auto metadata_update_policy = clone_metadata_update_policy();
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/true,
      [client, metadata_update_policy](
          grpc::ClientContext* context,
          google::bigtable::admin::v2::UpdateBackupRequest const& request,
          grpc::CompletionQueue* cq) {
        metadata_update_policy.Setup(*context);
        return client->AsyncUpdateBackup(context, request, cq);
      },
      std::move(request));
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
      "DeleteBackup", status, true);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return {};
}

future<Status> TableAdmin::AsyncDeleteBackup(
    CompletionQueue& cq, google::bigtable::admin::v2::Backup const& backup) {
  grpc::Status status;
  btadmin::DeleteBackupRequest request;
  request.set_name(backup.name());
  auto client = client_;
  auto metadata_update_policy = clone_metadata_update_policy();
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, metadata_update_policy](
                 grpc::ClientContext* context,
                 google::bigtable::admin::v2::DeleteBackupRequest const&
                     request,
                 grpc::CompletionQueue* cq) {
               metadata_update_policy.Setup(*context);

               return client->AsyncDeleteBackup(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<google::protobuf::Empty>> r) {
        return r.get().status();
      });
}

Status TableAdmin::DeleteBackup(std::string const& cluster_id,
                                std::string const& backup_id) {
  grpc::Status status;
  btadmin::DeleteBackupRequest request;
  request.set_name(instance_name() + "/clusters/" + cluster_id + "/backups/" +
                   backup_id);

  MetadataUpdatePolicy metadata_update_policy(request.name(),
                                              MetadataParamTypes::NAME);

  auto result = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy, &AdminClient::DeleteBackup, request,
      "DeleteBackup", status, true);
  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }

  return {};
}

future<Status> TableAdmin::AsyncDeleteBackup(CompletionQueue& cq,
                                             std::string const& cluster_id,
                                             std::string const& backup_id) {
  grpc::Status status;
  btadmin::DeleteBackupRequest request;
  request.set_name(instance_name() + "/clusters/" + cluster_id + "/backups/" +
                   backup_id);
  auto client = client_;
  auto metadata_update_policy = clone_metadata_update_policy();
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, metadata_update_policy](
                 grpc::ClientContext* context,
                 google::bigtable::admin::v2::DeleteBackupRequest const&
                     request,
                 grpc::CompletionQueue* cq) {
               metadata_update_policy.Setup(*context);

               return client->AsyncDeleteBackup(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<google::protobuf::Empty>> r) {
        return r.get().status();
      });
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
        &AdminClient::ListBackups, request, "TableAdmin", status, true);

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

future<StatusOr<std::vector<google::bigtable::admin::v2::Backup>>>
TableAdmin::AsyncListBackups(CompletionQueue& cq,
                             ListBackupsParams const& params) {
  auto client = client_;
  btadmin::ListBackupsRequest request = params.AsProto(instance_name());
  return internal::StartAsyncRetryMultiPage(
      __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      clone_metadata_update_policy(),
      [client](grpc::ClientContext* context,
               btadmin::ListBackupsRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncListBackups(context, request, cq);
      },
      std::move(request), std::vector<btadmin::Backup>(),
      [](std::vector<btadmin::Backup> acc,
         btadmin::ListBackupsResponse const& response) {
        std::move(response.backups().begin(), response.backups().end(),
                  std::back_inserter(acc));
        return acc;
      },
      cq);
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
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();
  return AsyncRestoreTable(cq, params)
      .then(
          [cq](future<StatusOr<google::bigtable::admin::v2::Table>> f) mutable {
            cq.Shutdown();
            return f.get();
          })
      .get();
}

future<StatusOr<google::bigtable::admin::v2::Table>>
TableAdmin::AsyncRestoreTable(CompletionQueue& cq,
                              RestoreTableParams const& params) {
  MetadataUpdatePolicy metadata_update_policy(instance_name(),
                                              MetadataParamTypes::PARENT);
  auto client = client_;
  return internal::AsyncStartPollAfterRetryUnaryRpc<
      google::bigtable::admin::v2::Table>(
      __func__, clone_polling_policy(), clone_rpc_retry_policy(),
      clone_rpc_backoff_policy(), internal::ConstantIdempotencyPolicy(false),
      metadata_update_policy, client,
      [client](grpc::ClientContext* context,
               google::bigtable::admin::v2::RestoreTableRequest const& request,
               grpc::CompletionQueue* cq) {
        return client->AsyncRestoreTable(context, request, cq);
      },
      params.AsProto(instance_name()), cq);
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
  auto result = ClientUtils::MakeNonIdemponentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::ModifyColumnFamilies, request, "ModifyColumnFamilies",
      status);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return result;
}

future<StatusOr<btadmin::Table>> TableAdmin::AsyncModifyColumnFamilies(
    CompletionQueue& cq, std::string const& table_id,
    std::vector<ColumnFamilyModification> modifications) {
  btadmin::ModifyColumnFamiliesRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  for (auto& m : modifications) {
    *request.add_modifications() = std::move(m).as_proto();
  }

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/true,
      [client, name](grpc::ClientContext* context,
                     btadmin::ModifyColumnFamiliesRequest const& request,
                     grpc::CompletionQueue* cq) {
        MetadataUpdatePolicy(name, MetadataParamTypes::NAME).Setup(*context);
        return client->AsyncModifyColumnFamilies(context, request, cq);
      },
      std::move(request));
}

Status TableAdmin::DropRowsByPrefix(std::string const& table_id,
                                    std::string row_key_prefix) {
  grpc::Status status;
  btadmin::DropRowRangeRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_row_key_prefix(std::move(row_key_prefix));
  MetadataUpdatePolicy metadata_update_policy(name, MetadataParamTypes::NAME);
  ClientUtils::MakeNonIdemponentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::DropRowRange, request, "DropRowByPrefix", status);

  return google::cloud::MakeStatusFromRpcError(status);
}

future<Status> TableAdmin::AsyncDropRowsByPrefix(CompletionQueue& cq,
                                                 std::string const& table_id,
                                                 std::string row_key_prefix) {
  google::bigtable::admin::v2::DropRowRangeRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_row_key_prefix(std::move(row_key_prefix));

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, name](grpc::ClientContext* context,
                            btadmin::DropRowRangeRequest const& request,
                            grpc::CompletionQueue* cq) {
               MetadataUpdatePolicy(name, MetadataParamTypes::NAME)
                   .Setup(*context);
               return client->AsyncDropRowRange(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<google::protobuf::Empty>> r) {
        return r.get().status();
      });
}

google::cloud::future<StatusOr<Consistency>> TableAdmin::WaitForConsistency(
    std::string const& table_id, std::string const& consistency_token) {
  CompletionQueue cq;
  std::thread([](CompletionQueue cq) { cq.Run(); }, cq).detach();

  return AsyncWaitForConsistency(cq, table_id, consistency_token)
      .then([cq](future<StatusOr<Consistency>> f) mutable {
        cq.Shutdown();
        return f.get();
      });
}

google::cloud::future<StatusOr<Consistency>>
TableAdmin::AsyncWaitForConsistency(CompletionQueue& cq,
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
  ClientUtils::MakeNonIdemponentCall(
      *client_, clone_rpc_retry_policy(), metadata_update_policy,
      &AdminClient::DropRowRange, request, "DropAllRows", status);

  return google::cloud::MakeStatusFromRpcError(status);
}

future<Status> TableAdmin::AsyncDropAllRows(CompletionQueue& cq,
                                            std::string const& table_id) {
  google::bigtable::admin::v2::DropRowRangeRequest request;
  auto name = TableName(table_id);
  request.set_name(name);
  request.set_delete_all_data_from_table(true);

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, name](grpc::ClientContext* context,
                            btadmin::DropRowRangeRequest const& request,
                            grpc::CompletionQueue* cq) {
               MetadataUpdatePolicy(name, MetadataParamTypes::NAME)
                   .Setup(*context);
               return client->AsyncDropRowRange(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<google::protobuf::Empty>> r) {
        return r.get().status();
      });
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
      "GenerateConsistencyToken", status, true);

  if (!status.ok()) {
    return google::cloud::MakeStatusFromRpcError(status);
  }
  return std::move(*response.mutable_consistency_token());
}

future<StatusOr<std::string>> TableAdmin::AsyncGenerateConsistencyToken(
    CompletionQueue& cq, std::string const& table_id) {
  btadmin::GenerateConsistencyTokenRequest request;
  auto name = TableName(table_id);
  request.set_name(name);

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, name](
                 grpc::ClientContext* context,
                 btadmin::GenerateConsistencyTokenRequest const& request,
                 grpc::CompletionQueue* cq) {
               MetadataUpdatePolicy(name, MetadataParamTypes::NAME)
                   .Setup(*context);
               return client->AsyncGenerateConsistencyToken(context, request,
                                                            cq);
             },
             std::move(request))
      .then([](future<StatusOr<btadmin::GenerateConsistencyTokenResponse>> fut)
                -> StatusOr<std::string> {
        auto result = fut.get();
        if (!result) {
          return result.status();
        }
        return std::move(*result->mutable_consistency_token());
      });
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
      "CheckConsistency", status, true);

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
             /*is_idempotent=*/true,
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
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &AdminClient::GetIamPolicy, request, "GetIamPolicy", status, true);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

future<StatusOr<google::iam::v1::Policy>> TableAdmin::AsyncGetIamPolicy(
    CompletionQueue& cq, std::string const& table_id) {
  ::google::iam::v1::GetIamPolicyRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/true,
      [client, resource](grpc::ClientContext* context,
                         ::google::iam::v1::GetIamPolicyRequest const& request,
                         grpc::CompletionQueue* cq) {
        MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE)
            .Setup(*context);
        return client->AsyncGetIamPolicy(context, request, cq);
      },
      std::move(request));
}

StatusOr<google::iam::v1::Policy> TableAdmin::SetIamPolicy(
    std::string const& table_id, google::iam::v1::Policy const& iam_policy) {
  grpc::Status status;
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);
  *request.mutable_policy() = iam_policy;

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto proto = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &AdminClient::SetIamPolicy, request, "SetIamPolicy", status, true);

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return proto;
}

future<StatusOr<google::iam::v1::Policy>> TableAdmin::AsyncSetIamPolicy(
    CompletionQueue& cq, std::string const& table_id,
    google::iam::v1::Policy const& iam_policy) {
  ::google::iam::v1::SetIamPolicyRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);
  *request.mutable_policy() = iam_policy;

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
      cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      /*is_idempotent=*/false,
      [client, resource](grpc::ClientContext* context,
                         ::google::iam::v1::SetIamPolicyRequest const& request,
                         grpc::CompletionQueue* cq) {
        MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE)
            .Setup(*context);
        return client->AsyncSetIamPolicy(context, request, cq);
      },
      std::move(request));
}

StatusOr<std::vector<std::string>> TableAdmin::TestIamPermissions(
    std::string const& table_id, std::vector<std::string> const& permissions) {
  grpc::Status status;
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);

  // Copy the policies in effect for the operation.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();

  for (auto& permission : permissions) {
    request.add_permissions(permission);
  }

  MetadataUpdatePolicy metadata_update_policy(resource,
                                              MetadataParamTypes::RESOURCE);

  auto response = ClientUtils::MakeCall(
      *(client_), *rpc_policy, *backoff_policy, metadata_update_policy,
      &AdminClient::TestIamPermissions, request, "TestIamPermissions", status,
      true);

  std::vector<std::string> resource_permissions;

  for (auto& permission : *response.mutable_permissions()) {
    resource_permissions.push_back(permission);
  }

  if (!status.ok()) {
    return MakeStatusFromRpcError(status);
  }

  return resource_permissions;
}

future<StatusOr<std::vector<std::string>>> TableAdmin::AsyncTestIamPermissions(
    CompletionQueue& cq, std::string const& table_id,
    std::vector<std::string> const& permissions) {
  ::google::iam::v1::TestIamPermissionsRequest request;
  auto resource = TableName(table_id);
  request.set_resource(resource);
  for (auto& permission : permissions) {
    request.add_permissions(permission);
  }

  auto client = client_;
  return google::cloud::internal::StartRetryAsyncUnaryRpc(
             cq, __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             /*is_idempotent=*/true,
             [client, resource](
                 grpc::ClientContext* context,
                 ::google::iam::v1::TestIamPermissionsRequest const& request,
                 grpc::CompletionQueue* cq) {
               MetadataUpdatePolicy(resource, MetadataParamTypes::RESOURCE)
                   .Setup(*context);
               return client->AsyncTestIamPermissions(context, request, cq);
             },
             std::move(request))
      .then([](future<StatusOr<::google::iam::v1::TestIamPermissionsResponse>>
                   response_fut) -> StatusOr<std::vector<std::string>> {
        auto response = response_fut.get();
        if (!response) {
          return response.status();
        }
        std::vector<std::string> res;
        res.reserve(response->permissions_size());
        std::move(response->mutable_permissions()->begin(),
                  response->mutable_permissions()->end(),
                  std::back_inserter(res));
        return res;
      });
}

std::string TableAdmin::InstanceName() const {
  return "projects/" + client_->project() + "/instances/" + instance_id_;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
