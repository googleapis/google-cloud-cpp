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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_H_

#include "google/cloud/spanner/version.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * A class that represents a Session.
 */
class Session {
 public:
  Session(std::string session_name) noexcept
      : session_name_(std::move(session_name)) {}

  // Not copyable or moveable.
  Session(Session const&) = delete;
  Session& operator=(Session const&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  std::string const& session_name() const { return session_name_; }

 private:
  std::string session_name_;
};

/**
 * A `SessionHolder` is a unique_ptr with a custom deleter that normally
 * returns the `Session` to the pool it came from (although in some cases it
 * just deletes the `Session` - see `MakeDissociatedSessionHolder`)
 */
using SessionHolder = std::unique_ptr<Session, std::function<void(Session*)>>;

/**
 * Returns a `SessionHolder` for a new `Session` that is not associated with
 * any pool; it just deletes the `Session`. This is for use in special cases
 * like partitioned operations where the `Session` may be used on multiple
 * machines and should not be returned to the pool.
 */
template <typename... Args>
SessionHolder MakeDissociatedSessionHolder(Args&&... args) {
  return SessionHolder(new Session(std::forward<Args>(args)...),
                       std::default_delete<Session>());
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_H_
