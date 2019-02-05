// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_ADMIN_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/column_family.h"
#include "google/cloud/bigtable/internal/async_check_consistency.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_config.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <future>
#include <memory>
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

/**
 * Implements the API to administer tables instance a Cloud Bigtable instance.
 */
class TableAdmin {
 public:
  // Make the class bigtable::TableAdmin friend of this class, So private
  // functions of this class can be accessed
  friend class bigtable::TableAdmin;
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
   */
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id)
      : client_(std::move(client)),
        instance_id_(std::move(instance_id)),
        instance_name_(InstanceName()),
        rpc_retry_policy_(
            DefaultRPCRetryPolicy(internal::kBigtableTableAdminLimits)),
        rpc_backoff_policy_(
            DefaultRPCBackoffPolicy(internal::kBigtableTableAdminLimits)),
        metadata_update_policy_(instance_name(), MetadataParamTypes::PARENT),
        polling_policy_(
            DefaultPollingPolicy(internal::kBigtableTableAdminLimits)) {}

  /**
   * Create a new TableAdmin using explicit policies to handle RPC errors.
   *
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
   * @param policies the set of policy overrides for this object.
   * @tparam Policies the types of the policies to override, the types must
   *     derive from one of the following types:
   *     - `RPCBackoffPolicy` how to backoff from a failed RPC. Currently only
   *       `ExponentialBackoffPolicy` is implemented. You can also create your
   *       own policies that backoff using a different algorithm.
   *     - `RPCRetryPolicy` for how long to retry failed RPCs. Use
   *       `LimitedErrorCountRetryPolicy` to limit the number of failures
   *       allowed. Use `LimitedTimeRetryPolicy` to bound the time for any
   *       request. You can also create your own policies that combine time and
   *       error counts.
   *     - `PollingPolicy` for how long will the class wait for
   *       `google.longrunning.Operation` to complete. This class combines both
   *       the backoff policy for checking long running operations and the
   *       retry policy.
   *
   * @see GenericPollingPolicy, ExponentialBackoffPolicy,
   *     LimitedErrorCountRetryPolicy, LimitedTimeRetryPolicy.
   */
  template <typename... Policies>
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id,
             Policies&&... policies)
      : TableAdmin(std::move(client), std::move(instance_id)) {
    ChangePolicies(std::forward<Policies>(policies)...);
  }

  std::string const& project() const { return client_->project(); }
  std::string const& instance_id() const { return instance_id_; }
  std::string const& instance_name() const { return instance_name_; }

  //@{
  /**
   * @name No exception versions of TableAdmin::*
   *
   * These functions provide the same functionality as their counterparts in the
   * `bigtable::TableAdmin` class, but do not raise exceptions on errors,
   * instead they return the error on the status parameter.
   */
  google::bigtable::admin::v2::Table CreateTable(std::string table_id,
                                                 TableConfig config,
                                                 grpc::Status& status);

  /**
   * Make an asynchronous request to create the table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object.  The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param config the initial schema for the table.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::admin::v2::Table&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&,
                    google::bigtable::admin::v2::Table&, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncCreateTable(CompletionQueue& cq,
                                                   Functor&& callback,
                                                   std::string table_id,
                                                   TableConfig config) {
    auto request = std::move(config).as_proto();
    request.set_parent(instance_name());
    request.set_table_id(std::move(table_id));

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncCreateTable)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncCreateTable)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy_,
        client_, &AdminClient::AsyncCreateTable, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  std::vector<google::bigtable::admin::v2::Table> ListTables(
      google::bigtable::admin::v2::Table::View view, grpc::Status& status);

  google::bigtable::admin::v2::Table GetTable(
      std::string const& table_id, grpc::Status& status,
      google::bigtable::admin::v2::Table::View view =
          google::bigtable::admin::v2::Table::SCHEMA_VIEW);

  /**
   * Make an asynchronous request to get the table metadata.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param mut the mutation to apply.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::v2::MutateRowResponse&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&,
                    google::bigtable::admin::v2::Table&, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncGetTable(
      CompletionQueue& cq, Functor&& callback, std::string const& table_id,
      google::bigtable::admin::v2::Table::View view) {
    google::bigtable::admin::v2::GetTableRequest request;
    request.set_name(TableName(table_id));
    request.set_view(view);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncGetTable)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncGetTable)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(true), metadata_update_policy_,
        client_, &AdminClient::AsyncGetTable, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  void DeleteTable(std::string const& table_id, grpc::Status& status);

  /**
   * Make an asynchronous request to delete the table.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object.  The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param config the initial schema of the table.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  void AsyncDeleteTable(CompletionQueue& cq, Functor&& callback,
                        std::string const& table_id, TableConfig config) {
    google::bigtable::admin::v2::DeleteTableRequest request;
    request.set_name(TableName(table_id));

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncDeleteTable)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncDeleteTable)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy_,
        client_, &AdminClient::AsyncDeleteTable, std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    retry->Start(cq);
  }

  google::bigtable::admin::v2::Table ModifyColumnFamilies(
      std::string const& table_id,
      std::vector<ColumnFamilyModification> modifications,
      grpc::Status& status);

  void DropRowsByPrefix(std::string const& table_id, std::string row_key_prefix,
                        grpc::Status& status);

  void DropAllRows(std::string const& table_id, grpc::Status& status);

  google::bigtable::admin::v2::Snapshot GetSnapshot(
      bigtable::ClusterId const& cluster_id,
      bigtable::SnapshotId const& snapshot_id, grpc::Status& status);

  std::string GenerateConsistencyToken(std::string const& table_id,
                                       grpc::Status& status);

  bool CheckConsistency(bigtable::TableId const& table_id,
                        bigtable::ConsistencyToken const& consistency_token,
                        grpc::Status& status);

  /**
   * Asynchronously wait for replication to catch up.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * This function asks for a consistency token, and polls Cloud Bigtable until
   * the replication has caught up to that consistency token, or until the
   * polling policy has expired.
   *
   * When the replication catches up the callback receives a `grpc::Status::OK`.
   *
   * If the policy expires before the replication catches up with the
   * consistency token then the callback receives `grpc::StatusCode::UNKNOWN`
   * status code.
   *
   * After this function returns OK you can be sure that all mutations which
   * have been applied before this call have made it to their replicas.
   *
   * @see https://cloud.google.com/bigtable/docs/replication-overview for an
   * overview of Cloud Bigtable replication.
   *
   * @param table_id the table to wait on
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. The
   *     replication will have caught up if status received by this callback is
   *     OK. It must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, CompletionQueue&, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncAwaitConsistency(
      CompletionQueue& cq, Functor&& callback,
      bigtable::TableId const& table_id) {
    auto op = std::make_shared<internal::AsyncAwaitConsistency>(
        __func__, polling_policy_->clone(), rpc_retry_policy_->clone(),
        rpc_backoff_policy_->clone(),
        MetadataUpdatePolicy(instance_name(), MetadataParamTypes::NAME,
                             table_id.get()),
        client_, TableName(table_id.get()));
    return op->Start(cq, std::forward<Functor>(callback));
  }

  void DeleteSnapshot(bigtable::ClusterId const& cluster_id,
                      bigtable::SnapshotId const& snapshot_id,
                      grpc::Status& status);

  /**
   * List snapshots in the given instance.
   *
   * @param status status of the operation
   * @param cluster_id the name of the cluster for which snapshots should be
   * listed.
   * @return vector containing the snapshots for the given cluster.
   */
  std::vector<google::bigtable::admin::v2::Snapshot> ListSnapshots(
      grpc::Status& status,
      bigtable::ClusterId const& cluster_id = bigtable::ClusterId("-"));

  /**
   * Make an asynchronous request to modify the column families of a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object.  The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param modifications the list of modifications to the schema.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::v2::Table&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&,
                    google::bigtable::admin::v2::Table&, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncModifyColumnFamilies(
      CompletionQueue& cq, Functor&& callback, std::string const& table_id,
      std::vector<ColumnFamilyModification> modifications) {
    google::bigtable::admin::v2::ModifyColumnFamiliesRequest request;
    request.set_name(TableName(table_id));
    for (auto& m : modifications) {
      *request.add_modifications() = std::move(m).as_proto();
    }
    MetadataUpdatePolicy metadata_update_policy(
        instance_name(), MetadataParamTypes::NAME, table_id);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncModifyColumnFamilies)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncModifyColumnFamilies)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy,
        client_, &AdminClient::AsyncModifyColumnFamilies, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  /**
   * Make an asynchronous request to delete all the rows that start with a given
   * prefix.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param row_key_prefix drop any rows that start with this prefix.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncDropRowsByPrefix(
      CompletionQueue& cq, Functor&& callback, std::string const& table_id,
      std::string row_key_prefix) {
    google::bigtable::admin::v2::DropRowRangeRequest request;
    request.set_name(TableName(table_id));
    request.set_row_key_prefix(std::move(row_key_prefix));
    MetadataUpdatePolicy metadata_update_policy(
        instance_name(), MetadataParamTypes::NAME, table_id);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncDropRowRange)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncDropRowRange)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy,
        client_, &AdminClient::AsyncDropRowRange, std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    return retry->Start(cq);
  }

  /**
   * Make an asynchronous request to delete all the rows of a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncDropAllRows(
      CompletionQueue& cq, Functor&& callback, std::string const& table_id) {
    google::bigtable::admin::v2::DropRowRangeRequest request;
    request.set_name(TableName(table_id));
    request.set_delete_all_data_from_table(true);
    MetadataUpdatePolicy metadata_update_policy(
        instance_name(), MetadataParamTypes::NAME, table_id);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncDropRowRange)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncDropRowRange)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy,
        client_, &AdminClient::AsyncDropRowRange, std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    return retry->Start(cq);
  }

  /**
   * Make an asynchronous request to get information about a single snapshot.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @warning This is a private alpha release of Cloud Bigtable snapshots. This
   * feature is not currently available to most Cloud Bigtable customers. This
   * feature might be changed in backward-incompatible ways and is not
   * recommended for production use. It is not subject to any SLA or deprecation
   * policy.
   *
   * @param cluster_id the cluster id to which snapshot is associated.
   * @param snapshot_id the id of the snapshot.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<
   *         Functor, google::bigtable::admin::v2::Snapshot&,
   *         grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   */
  template <typename Functor,
            typename std::enable_if<google::cloud::internal::is_invocable<
                                        Functor, CompletionQueue&,
                                        google::bigtable::admin::v2::Snapshot&,
                                        grpc::Status&>::value,
                                    int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncGetSnapshot(
      CompletionQueue& cq, Functor&& callback,
      bigtable::ClusterId const& cluster_id,
      bigtable::SnapshotId const& snapshot_id) {
    google::bigtable::admin::v2::GetSnapshotRequest request;
    request.set_name(SnapshotName(cluster_id, snapshot_id));
    MetadataUpdatePolicy metadata_update_policy(
        instance_name(), MetadataParamTypes::NAME, cluster_id, snapshot_id);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncGetSnapshot)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncGetSnapshot)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     Functor>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy,
        client_, &AdminClient::AsyncGetSnapshot, std::move(request),
        std::forward<Functor>(callback));
    return retry->Start(cq);
  }

  /**
   * Make an asynchronous request to delete a snapshot.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @warning This is a private alpha release of Cloud Bigtable snapshots. This
   * feature is not currently available to most Cloud Bigtable customers. This
   * feature might be changed in backward-incompatible ways and is not
   * recommended for production use. It is not subject to any SLA or deprecation
   * policy.
   *
   * @param cluster_id the cluster id to which snapshot is associated.
   * @param snapshot_id the id of the snapshot.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param callback a functor to be called when the operation completes. It
   *     must satisfy (using C++17 types):
   *     static_assert(std::is_invocable_v<Functor, grpc::Status const&>);
   * @return a handle to the submitted operation
   *
   * @tparam Functor the type of the callback.
   *
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> AsyncDeleteSnapshot(
      CompletionQueue& cq, Functor&& callback,
      bigtable::ClusterId const& cluster_id,
      bigtable::SnapshotId const& snapshot_id) {
    google::bigtable::admin::v2::DeleteSnapshotRequest request;
    request.set_name(SnapshotName(cluster_id, snapshot_id));
    MetadataUpdatePolicy metadata_update_policy(
        instance_name(), MetadataParamTypes::NAME, cluster_id, snapshot_id);

    static_assert(internal::ExtractMemberFunctionType<decltype(
                      &AdminClient::AsyncDeleteSnapshot)>::value,
                  "Cannot extract member function type");
    using MemberFunction =
        typename internal::ExtractMemberFunctionType<decltype(
            &AdminClient::AsyncDeleteSnapshot)>::MemberFunction;

    using Retry =
        internal::AsyncRetryUnaryRpc<AdminClient, MemberFunction,
                                     internal::ConstantIdempotencyPolicy,
                                     internal::EmptyResponseAdaptor<Functor>>;

    auto retry = std::make_shared<Retry>(
        __func__, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
        internal::ConstantIdempotencyPolicy(false), metadata_update_policy,
        client_, &AdminClient::AsyncDeleteSnapshot, std::move(request),
        internal::EmptyResponseAdaptor<Functor>(
            std::forward<Functor>(callback)));
    return retry->Start(cq);
  }
  //@}

 private:
  //@{
  /// @name Helper functions to implement constructors with changed policies.
  void ChangePolicy(RPCRetryPolicy& policy) {
    rpc_retry_policy_ = policy.clone();
  }

  void ChangePolicy(RPCBackoffPolicy& policy) {
    rpc_backoff_policy_ = policy.clone();
  }

  void ChangePolicy(PollingPolicy& policy) { polling_policy_ = policy.clone(); }

  template <typename Policy, typename... Policies>
  void ChangePolicies(Policy&& policy, Policies&&... policies) {
    ChangePolicy(policy);
    ChangePolicies(std::forward<Policies>(policies)...);
  }
  void ChangePolicies() {}
  //@}

  /// Compute the fully qualified instance name.
  std::string InstanceName() const;

  /// Return the fully qualified name of a table in this object's instance.
  std::string TableName(std::string const& table_id) const {
    return instance_name() + "/tables/" + table_id;
  }

  /// Return the fully qualified name of a snapshot.
  std::string SnapshotName(bigtable::ClusterId const& cluster_id,
                           bigtable::SnapshotId const& snapshot_id) {
    return instance_name() + "/clusters/" + cluster_id.get() + "/snapshots/" +
           snapshot_id.get();
  }

  /// Return the fully qualified name of a Cluster.
  std::string ClusterName(bigtable::ClusterId const& cluster_id) {
    return instance_name() + "/clusters/" + cluster_id.get();
  }

  bool WaitForConsistencyCheckHelper(
      bigtable::TableId const& table_id,
      bigtable::ConsistencyToken const& consistency_token,
      grpc::Status& status);

 private:
  std::shared_ptr<AdminClient> client_;
  std::string instance_id_;
  std::string instance_name_;
  std::shared_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<PollingPolicy> polling_policy_;
};

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_ADMIN_H_
