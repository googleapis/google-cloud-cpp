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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace spanner_proto = ::google::spanner::v1;

StatusOr<ResultSet> Client::Read(std::string const& /*table*/,
                                 KeySet const& /*keys*/,
                                 std::vector<std::string> const& /*columns*/,
                                 ReadOptions const& /*read_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::Read(
    Transaction::SingleUseOptions const& /*transaction_options*/,
    std::string const& /*table*/, KeySet const& /*keys*/,
    std::vector<std::string> const& /*columns*/,
    ReadOptions const& /*read_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::Read(Transaction const& /*transaction*/,
                                 std::string const& /*table*/,
                                 KeySet const& /*keys*/,
                                 std::vector<std::string> const& /*columns*/,
                                 ReadOptions const& /*read_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::Read(SqlPartition const& /*partition*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<SqlPartition>> Client::PartitionRead(
    Transaction const& /*transaction*/, std::string const& /*table*/,
    spanner::KeySet const& /*keys*/,
    std::vector<std::string> const& /*columns*/,
    ReadOptions const& /*read_options*/,
    PartitionOptions const& /*partition_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(
    Transaction::SingleUseOptions const& /*transaction_options*/,
    SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(Transaction const& /*transaction*/,
                                       SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(SqlPartition const& /* partition */) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<SqlPartition>> Client::PartitionQuery(
    Transaction const& /*transaction*/, SqlStatement const& /*statement*/,
    PartitionOptions const& /*partition_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

std::vector<StatusOr<spanner_proto::ResultSetStats>> Client::ExecuteBatchDml(
    Transaction const& /*transaction*/,
    std::vector<SqlStatement> const& /*statements*/) {
  return {Status(StatusCode::kUnimplemented, "not implemented")};
}

// returns a lower bound on the number of modified rows
StatusOr<std::int64_t> Client::ExecutePartitionedDml(
    SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<CommitResult> Client::Commit(
    Transaction const& /*transaction*/,
    std::vector<Mutation> const& /*mutations*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

Status Client::Rollback(Transaction const& /*transaction*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<Client> MakeClient(ClientOptions const& client_options) {
  return Client(internal::CreateDefaultSpannerStub(client_options));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
