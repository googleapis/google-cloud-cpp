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
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <cstdint>
#include <memory>
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
  // Creates a ConnectionImpl that will talk to the specified `database` using
  // the given `stub`. We can test this class by injecting in a mock `stub`.
  explicit ConnectionImpl(std::string database,
                          std::shared_ptr<internal::SpannerStub> stub)
      : database_(std::move(database)), stub_(std::move(stub)) {}

  StatusOr<ResultSet> Read(ReadParams rp) override;
  StatusOr<ResultSet> ExecuteSql(ExecuteSqlParams esp) override;
  StatusOr<CommitResult> Commit(CommitParams cp) override;
  Status Rollback(RollbackParams rp) override;

 private:
  class SessionHolder {
   public:
    SessionHolder(std::string session, ConnectionImpl* conn) noexcept
        : session_(std::move(session)), conn_(conn) {}
    ~SessionHolder() { conn_->sessions_.emplace_back(std::move(session_)); }

    std::string const& session_name() const { return session_; }

   private:
    std::string session_;
    ConnectionImpl* conn_;
  };
  friend class SessionHolder;
  StatusOr<SessionHolder> GetSession();

  /// Implementation details for Read.
  StatusOr<ResultSet> Read(google::spanner::v1::TransactionSelector& s,
                           ReadParams rp);

  /// Implementation details for ExecuteSql
  StatusOr<ResultSet> ExecuteSql(google::spanner::v1::TransactionSelector& s,
                                 std::int64_t seqno, ExecuteSqlParams esp);

  /// Implementation details for Commit.
  StatusOr<CommitResult> Commit(google::spanner::v1::TransactionSelector& s,
                                CommitParams cp);

  /// Implementation details for Rollback.
  Status Rollback(google::spanner::v1::TransactionSelector& s);

  std::string database_;
  std::shared_ptr<internal::SpannerStub> stub_;

  // The current session pool.
  // TODO(#307) - improve session refresh and expiration.
  std::vector<std::string> sessions_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_
