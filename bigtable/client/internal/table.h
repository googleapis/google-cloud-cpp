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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_TABLE_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_TABLE_H_

#include "bigtable/client/data_client.h"
#include "bigtable/client/filters.h"
#include "bigtable/client/idempotent_mutation_policy.h"
#include "bigtable/client/internal/unary_rpc_utils.h"
#include "bigtable/client/metadata_update_policy.h"
#include "bigtable/client/mutations.h"
#include "bigtable/client/read_modify_write_rule.h"
#include "bigtable/client/row_reader.h"
#include "bigtable/client/row_set.h"
#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Return the full table name.
 *
 * The full table name is:
 *
 * `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
 *
 * Where the project id and instance id come from the @p client parameter.
 */
inline std::string TableName(std::shared_ptr<DataClient> client,
                             std::string const& table_id) {
  return InstanceName(std::move(client)) + "/tables/" + table_id;
}

/// A simple wrapper to represent the response from `Table::SampleRowKeys()`.
struct RowKeySample {
  std::string row_key;
  std::int64_t offset_bytes;
};

/**
 * This namespace contains implementations of the API that do not raise
 * exceptions. It is subject to change without notice, and therefore, not
 * recommended for direct use by applications.
 *
 * Provide APIs to access and modify data in a Cloud Bigtable table.
 */
namespace noex {
class Table {
 public:
  Table(std::shared_ptr<DataClient> client, std::string const& table_id)
      : client_(std::move(client)),
        table_name_(TableName(client_, table_id)),
        rpc_retry_policy_(bigtable::DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy()),
        metadata_update_policy_(table_name(), MetadataParamTypes::TABLE_NAME),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()) {}

  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename IdempotentMutationPolicy>
  Table(std::shared_ptr<DataClient> client, std::string const& table_id,
        RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
        IdempotentMutationPolicy idempotent_mutation_policy)
      : client_(std::move(client)),
        table_name_(TableName(client_, table_id)),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        metadata_update_policy_(table_name(), MetadataParamTypes::TABLE_NAME),
        idempotent_mutation_policy_(idempotent_mutation_policy.clone()) {}

  std::string const& table_name() const { return table_name_; }

  //@{
  /**
   * @name No exception versions of Table::*
   *
   * These functions provide the same functionality as their counterparts in the
   * `bigtable::Table` class, but do not raise exceptions on errors, instead
   * they return the error on the status parameter.
   */
  std::vector<FailedMutation> Apply(SingleRowMutation&& mut);

  std::vector<FailedMutation> BulkApply(BulkMutation&& mut,
                                        grpc::Status& status);

  RowReader ReadRows(RowSet row_set, Filter filter,
                     bool raise_on_error = false);

  RowReader ReadRows(RowSet row_set, std::int64_t rows_limit, Filter filter,
                     bool raise_on_error = false);

  std::pair<bool, Row> ReadRow(std::string row_key, Filter filter,
                               grpc::Status& status);

  bool CheckAndMutateRow(std::string row_key, Filter filter,
                         std::vector<Mutation> true_mutations,
                         std::vector<Mutation> false_mutations,
                         grpc::Status& status);

  template <typename... Args>
  Row ReadModifyWriteRow(std::string row_key, grpc::Status& status,
                         bigtable::ReadModifyWriteRule rule, Args&&... rules) {
    ::google::bigtable::v2::ReadModifyWriteRowRequest request;
    request.set_table_name(table_name_);
    request.set_row_key(std::move(row_key));

    // Generate a better compile time error message than the default one
    // if the types do not match
    static_assert(
        internal::conjunction<
            std::is_convertible<Args, bigtable::ReadModifyWriteRule>...>::value,
        "The arguments passed to ReadModifyWriteRow(row_key,...) must be "
        "convertible to bigtable::ReadModifyWriteRule");

    // TODO(#336) - optimize this code by not copying the parameter pack.
    // Add first default rule
    *request.add_rules() = rule.as_proto_move();
    // Add if any additional rule is present
    std::initializer_list<bigtable::ReadModifyWriteRule> rule_list{
        std::forward<Args>(rules)...};
    for (auto args_rule : rule_list) {
      *request.add_rules() = args_rule.as_proto_move();
    }

    return CallReadModifyWriteRowRequest(request, status);
  }

  template <template <typename...> class Collection = std::vector>
  Collection<bigtable::RowKeySample> SampleRows(grpc::Status& status) {
    Collection<bigtable::RowKeySample> result;
    SampleRowsImpl(
        [&result](bigtable::RowKeySample rs) {
          result.emplace_back(std::move(rs));
        },
        [&result]() { result.clear(); }, status);
    return result;
  }

  //@}

 private:
  using RpcUtils = bigtable::internal::noex::UnaryRpcUtils<DataClient>;
  using StubType = RpcUtils::StubType;
  /**
   * Send request ReadModifyWriteRowRequest to modify the row and get it back
   */
  Row CallReadModifyWriteRowRequest(
      ::google::bigtable::v2::ReadModifyWriteRowRequest request,
      grpc::Status& status);

  /**
   * Refactor implementation to `.cc` file.
   *
   * Provides a compilation barrier so that the application is not
   * exposed to all the implementation details.
   *
   * @param inserter Function to insert the object to result.
   * @param clearer Function to clear the result object if RPC fails.
   */
  void SampleRowsImpl(std::function<void(bigtable::RowKeySample)> inserter,
                      std::function<void()> clearer, grpc::Status& status);

  std::shared_ptr<DataClient> client_;
  std::string table_name_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
};

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_INTERNAL_TABLE_H_
