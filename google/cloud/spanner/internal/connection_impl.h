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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_

#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/session_holder.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * A concrete `Connection` subclass that uses gRPC to actually talk to a real
 * Spanner instance. See `MakeConnection()` for a factory function that creates
 * and returns instances of this class.
 */
class ConnectionImpl : public Connection {
 public:
  // Creates a ConnectionImpl that will talk to the specified `db` using the
  // given `stub`. We can test this class by injecting in a mock `stub`.
  explicit ConnectionImpl(Database db,
                          std::shared_ptr<internal::SpannerStub> stub)
      : db_(std::move(db)), stub_(std::move(stub)) {}

  StatusOr<ResultSet> Read(ReadParams rp) override;
  StatusOr<std::vector<ReadPartition>> PartitionRead(
      PartitionReadParams prp) override;
  StatusOr<ResultSet> ExecuteSql(ExecuteSqlParams esp) override;
  StatusOr<std::vector<QueryPartition>> PartitionQuery(
      PartitionQueryParams) override;
  StatusOr<CommitResult> Commit(CommitParams cp) override;
  Status Rollback(RollbackParams rp) override;

 private:
  StatusOr<SessionHolder> GetSession();
  void ReleaseSession(std::string session);

  /// Implementation details for Read.
  StatusOr<ResultSet> Read(google::spanner::v1::TransactionSelector& s,
                           ReadParams rp);

  /// Implementation details for PartitionRead.
  StatusOr<std::vector<ReadPartition>> PartitionRead(
      google::spanner::v1::TransactionSelector& s, ReadParams const& rp,
      PartitionOptions partition_options);

  /// Implementation details for ExecuteSql
  StatusOr<ResultSet> ExecuteSql(google::spanner::v1::TransactionSelector& s,
                                 std::int64_t seqno, ExecuteSqlParams esp);

  /// Implementation details for PartitionQuery
  StatusOr<std::vector<QueryPartition>> PartitionQuery(
      google::spanner::v1::TransactionSelector& s, ExecuteSqlParams const& esp,
      PartitionOptions partition_options);

  /// Implementation details for Commit.
  StatusOr<CommitResult> Commit(google::spanner::v1::TransactionSelector& s,
                                CommitParams cp);

  /// Implementation details for Rollback.
  Status Rollback(google::spanner::v1::TransactionSelector& s);

  Database db_;
  std::shared_ptr<internal::SpannerStub> stub_;

  // The current session pool.
  // TODO(#307) - improve session refresh and expiration.
  std::mutex mu_;
  std::vector<std::string> sessions_;  // GUARDED_BY(mu_)
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_
