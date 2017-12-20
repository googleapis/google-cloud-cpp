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

#ifndef BIGTABLE_CLIENT_DATA_H_
#define BIGTABLE_CLIENT_DATA_H_

#include "bigtable/client/client_options.h"
#include "bigtable/client/idempotent_mutation_policy.h"
#include "bigtable/client/mutations.h"
#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"

#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class Table;

class ClientInterface {
 public:
  virtual ~ClientInterface() = default;

  // Create a Table object for use with the Data API. Never fails, all
  // error checking happens during operations.
  virtual std::unique_ptr<Table> Open(const std::string& table_id) = 0;

  // Access the stub to send RPC calls.
  virtual google::bigtable::v2::Bigtable::StubInterface& Stub() const = 0;
};

class Client : public ClientInterface {
 public:
  Client(const std::string& project, const std::string& instance,
         const ClientOptions& options)
      : project_(project),
        instance_(instance),
        credentials_(options.credentials()),
        channel_(
            grpc::CreateChannel(options.data_endpoint(), options.credentials())),
        bt_stub_(google::bigtable::v2::Bigtable::NewStub(channel_)) {}

  Client(const std::string& project, const std::string& instance)
      : Client(project, instance, ClientOptions()) {}

  std::unique_ptr<Table> Open(const std::string& table_id);

  google::bigtable::v2::Bigtable::StubInterface& Stub() const {
    return *bt_stub_;
  }

 private:
  std::string project_;
  std::string instance_;
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<google::bigtable::v2::Bigtable::StubInterface> bt_stub_;

  friend class Table;
};

class Table {
 public:
  /// Constructor with default policies
  Table(const ClientInterface* client, const std::string& table_name)
      : client_(client),
        table_name_(table_name),
        rpc_retry_policy_(bigtable::DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(bigtable::DefaultRPCBackoffPolicy()),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()) {}

  /// Constructor with explicit policies
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename IdempotentMutationPolicy>
  Table(const ClientInterface* client, const std::string& table_name,
        RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
        IdempotentMutationPolicy idempotent_mutation_policy)
      : client_(client),
        table_name_(table_name),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        idempotent_mutation_policy_(idempotent_mutation_policy.clone()) {}

  const std::string& table_name() const { return table_name_; }

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
  const ClientInterface* client_;
  std::string table_name_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::unique_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_DATA_H_
