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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_HOLDER_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_HOLDER_H_

#include "google/cloud/spanner/version.h"
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * A RAII class that releases a session to the pool when destroyed.
 */
class SessionHolder {
 public:
  SessionHolder() = default;
  SessionHolder(std::string session,
                std::function<void(std::string)> deleter) noexcept
      : session_(std::move(session)), deleter_(std::move(deleter)) {}

  // This class is move-only because we want only one destructor calling the
  // `deleter_`.
  SessionHolder(SessionHolder const&) = delete;
  SessionHolder& operator=(SessionHolder const&) = delete;

  SessionHolder(SessionHolder&& rhs)
      : session_(std::move(rhs.session_)), deleter_(std::move(rhs.deleter_)) {
    rhs.deleter_ = nullptr;
  }

  SessionHolder& operator=(SessionHolder&& rhs) {
    if (&rhs != this) {
      session_ = std::move(rhs.session_);
      deleter_ = std::move(rhs.deleter_);
      rhs.deleter_ = nullptr;
    }
    return *this;
  }

  ~SessionHolder() {
    if (deleter_) {
      deleter_(std::move(session_));
    }
  }

  std::string const& session_name() const { return session_; }

 private:
  std::string session_;
  std::function<void(std::string)> deleter_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_HOLDER_H_
