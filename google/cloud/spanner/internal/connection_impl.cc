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

#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/time.h"
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace spanner_proto = ::google::spanner::v1;

StatusOr<CommitResult> ConnectionImpl::Commit(Connection::CommitParams cp) {
  return internal::Visit(
      std::move(cp.transaction),
      [this, &cp](spanner_proto::TransactionSelector& s, std::int64_t) {
        return this->Commit(s, std::move(cp.mutations));
      });
}

StatusOr<ConnectionImpl::SessionHolder> ConnectionImpl::GetSession() {
  if (!sessions_.empty()) {
    std::string session = sessions_.back();
    sessions_.pop_back();
    return SessionHolder(std::move(session), this);
  }
  grpc::ClientContext context;
  google::spanner::v1::CreateSessionRequest request;
  request.set_database(database_);
  auto response = stub_->CreateSession(context, request);
  if (!response) {
    return response.status();
  }
  return SessionHolder(std::move(*response->mutable_name()), this);
}

StatusOr<CommitResult> ConnectionImpl::Commit(
    google::spanner::v1::TransactionSelector& s,
    std::vector<Mutation> mutations) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
  }
  google::spanner::v1::CommitRequest request;
  request.set_session(session->session_name());
  for (auto&& m : mutations) {
    *request.add_mutations() = std::move(m).as_proto();
  }
  if (!s.id().empty()) {
    request.set_transaction_id(s.id());
  } else if (s.has_begin()) {
    *request.mutable_single_use_transaction() = s.begin();
  } else if (s.has_single_use()) {
    *request.mutable_single_use_transaction() = s.single_use();
  }
  grpc::ClientContext context;
  auto response = stub_->Commit(context, request);
  if (!response) {
    return std::move(response).status();
  }
  CommitResult r;
  r.commit_timestamp = internal::FromProto(response->commit_timestamp());
  return r;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
