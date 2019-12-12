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

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include <atomic>
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
 *
 * This class is thread-safe.
 */
class Session {
 public:
  Session(std::string session_name, std::shared_ptr<SpannerStub> stub) noexcept
      : session_name_(std::move(session_name)),
        stub_(std::move(stub)),
        is_bad_(false) {}

  // Not copyable or moveable.
  Session(Session const&) = delete;
  Session& operator=(Session const&) = delete;
  Session(Session&&) = delete;
  Session& operator=(Session&&) = delete;

  std::string const& session_name() const { return session_name_; }

  // Note: the "bad" state only transitions from false to true.
  void set_bad() { is_bad_.store(true, std::memory_order_relaxed); }
  bool is_bad() const { return is_bad_.load(std::memory_order_relaxed); }

 private:
  friend class SessionPool;  // for access to stub()
  std::shared_ptr<SpannerStub> stub() const { return stub_; }

  std::string const session_name_;
  std::shared_ptr<SpannerStub> const stub_;
  std::atomic<bool> is_bad_;
};

/**
 * A `SessionHolder` is a shared_ptr with a custom deleter that normally
 * returns the `Session` to the pool it came from (although in some cases it
 * just deletes the `Session` - see `MakeDissociatedSessionHolder`)
 */
using SessionHolder = std::shared_ptr<Session>;

/**
 * Returns a `SessionHolder` for a new `Session` that is not associated with
 * any pool; it just deletes the `Session`. This is for use in special cases
 * like partitioned operations where the `Session` may be used on multiple
 * machines and should not be returned to the pool.
 */
SessionHolder MakeDissociatedSessionHolder(std::string session_name);

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_SESSION_H_
