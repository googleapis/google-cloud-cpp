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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_DATA_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_DATA_H_

#include "bigtable/client/client_options.h"
#include "bigtable/client/idempotent_mutation_policy.h"
#include "bigtable/client/mutations.h"
#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"

#include <google/bigtable/v2/bigtable.grpc.pb.h>

#include <absl/strings/str_cat.h>
#include <absl/strings/string_view.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class ClientInterface {
 public:
  virtual ~ClientInterface() = default;

  virtual std::string const& ProjectId() const = 0;
  virtual std::string const& InstanceId() const = 0;

  // Access the stub to send RPC calls.
  virtual google::bigtable::v2::Bigtable::StubInterface& Stub() const = 0;
};

/// Create the default implementation of ClientInterface.
std::shared_ptr<ClientInterface> CreateDefaultClient(std::string project_id,
                                                     std::string instance_id,
                                                     ClientOptions options);

inline std::string CreateInstanceName(std::shared_ptr<ClientInterface> client) {
  return absl::StrCat("projects/", client->ProjectId(), "/instances/",
                      client->InstanceId());
}

inline std::string CreateTableName(std::shared_ptr<ClientInterface> client,
                                   absl::string_view table_id) {
  return absl::StrCat(CreateInstanceName(std::move(client)), "/tables/", table_id);
}

class Table {
 public:
  /**
   * Constructor with default policies.
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + '/tables/' + table_id`.
   */
  Table(std::shared_ptr<ClientInterface> client, absl::string_view table_id)
      : client_(std::move(client)),
        table_name_(CreateTableName(client_, table_id)),
        rpc_retry_policy_(bigtable::DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy()),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()) {}

  /**
   * Constructor with default policies.
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + '/tables/' + table_id`.
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + '/tables/' + table_id`.
   * @param retry_policy the value of the RPCRetryPolicy, for example, the
   *     policy type may be `bigtable::RPCLimitedErrorCountRetryPolicy` which
   *     tolerates a maximum number of errors, the value controls how many.
   * @param backoff_policy the value of the RPCBackoffPolicy, for example, the
   *     policy type may be `bigtable::ExponentialBackoffPolicy` which will
   *     double the wait period on each failure, up to a limit.  The value
   *     controls the initial and maximum wait periods.
   * @param idempotent_mutation_policy the value of the
   *     IdempotentMutationPolicy.
   */
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename IdempotentMutationPolicy>
  Table(std::shared_ptr<ClientInterface> client, absl::string_view table_id,
        RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
        IdempotentMutationPolicy idempotent_mutation_policy)
      : client_(std::move(client)),
        table_name_(CreateTableName(client_, table_id)),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        idempotent_mutation_policy_(idempotent_mutation_policy.clone()) {}

  std::string const& table_name() const { return table_name_; }

  /**
   * Attempts to apply the mutation to a row.
   *
   * @param mut the mutation, notice that this function takes
   *     ownership (and then discards) the data in the mutation.
   *
   * @throws std::exception based on how the retry policy handles
   *     error conditions.
   */
  void Apply(SingleRowMutation&& mut);

  /**
   * Attempts to apply mutations to multiple rows.
   *
   * @param mut the mutations, notice that this function takes
   *     ownership (and then discards) the data in the mutation.
   *
   * @throws bigtable::MultipleMutationFailure based on how the retry policy
   *     handles error conditions.  Notice that not idempotent mutations that
   *     are not reported as successful or failed by the server are not sent
   *     to the server more than once, and are reported back with a OK status
   *     in the exception.
   */
  void BulkApply(BulkMutation&& mut);

 private:
  std::shared_ptr<ClientInterface> client_;
  std::string table_name_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::unique_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_DATA_H_
