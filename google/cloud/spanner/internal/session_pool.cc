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

#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/session.h"
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

StatusOr<std::unique_ptr<Session>> SessionPool::Allocate() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!sessions_.empty()) {
      auto session = std::move(sessions_.back());
      sessions_.pop_back();
      return {std::move(session)};
    }
  }

  auto sessions = manager_->CreateSessions(/*num_sessions=*/1);
  if (!sessions.ok()) {
    return std::move(sessions).status();
  }
  // TODO(#307) for now, assume CreateSessions() returns exactly one Session on
  // success (as we requested). Rewrite this to accommodate multiple sessions.
  return {std::move((*sessions)[0])};
}

void SessionPool::Release(std::unique_ptr<Session> session) {
  std::lock_guard<std::mutex> lk(mu_);
  sessions_.push_back(std::move(session));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
