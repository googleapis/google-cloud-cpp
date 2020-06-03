// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_READY_TOKEN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_READY_TOKEN_H

#include "google/cloud/future.h"
#include "google/cloud/version.h"
#include <deque>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * A token to request more data from a `source<T, E>`.
 *
 * Some instances of `source<T, E>` can have at most N (typically 1) calls
 * outstanding. Users of these `source<T, E>` objects must obtain a token before
 * calling `next()`. These tokens are returned as futures only satisfied when
 * the number of outstanding requests is within limits.
 *
 * This is a move-only class and the move constructor and move assignments
 * invalidate the source object.
 */
class ReadyToken {
 public:
  ReadyToken() = default;

  ReadyToken(ReadyToken const&) = delete;
  ReadyToken& operator=(ReadyToken const&) = delete;

  /// Move constructor, invalidates @p rhs
  ReadyToken(ReadyToken&& rhs) noexcept : value_(rhs.value_) {
    rhs.value_ = nullptr;
  }

  /// Move assignment, invalidates @p rhs
  ReadyToken& operator=(ReadyToken&& rhs) noexcept {
    value_ = rhs.value_;
    rhs.value_ = nullptr;
    return *this;
  }

  bool valid() const { return value_ != nullptr; }
  explicit operator bool() const { return valid(); }

 private:
  friend class ReadyTokenFlowControl;
  explicit ReadyToken(void const* v) : value_(v) {}

  void const* value_ = nullptr;
};

/**
 * Helper class to flow control based on `ReadyToken`.
 *
 * This class is used by `source<T, E>` implementations when they want to flow
 * control the number of outstanding `ReadyToken()` objects.
 *
 * @par Thread Safety
 *
 * The move constructor and move assignment operations are *not* thread-safe,
 * neither the source nor the destination object (for assignment) may be used
 * by more than one thread.
 *
 * Other member functions are thread-safe, more than one thread may call these
 * functions simultaneously.
 */
class ReadyTokenFlowControl {
 public:
  explicit ReadyTokenFlowControl(int max_outstanding = 1)
      : max_outstanding_(max_outstanding) {}

  ReadyTokenFlowControl(ReadyTokenFlowControl&& rhs) noexcept
      : current_outstanding_(rhs.current_outstanding_),
        max_outstanding_(rhs.max_outstanding_),
        pending_(std::move(rhs.pending_)) {}
  ReadyTokenFlowControl& operator=(ReadyTokenFlowControl&& rhs) noexcept {
    ReadyTokenFlowControl tmp(std::move(rhs));
    using std::swap;
    swap(current_outstanding_, tmp.current_outstanding_);
    swap(max_outstanding_, tmp.max_outstanding_);
    swap(pending_, tmp.pending_);
    return *this;
  }

  /// The maximum number of outstanding tokens.
  int max_outstanding() const { return max_outstanding_; }

  /**
   * Asynchronously acquire a new `ReadyToken`.
   *
   * The returned future is satisfied when/if there are less outstanding tokens
   * than max_outstanding().
   */
  future<ReadyToken> Acquire();

  /// Reclaim a token, return false if it was not created by this object.
  bool Release(ReadyToken token);

 private:
  std::mutex mu_;
  int current_outstanding_ = 0;
  int max_outstanding_ = 1;
  std::deque<promise<ReadyToken>> pending_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_SOURCE_READY_TOKEN_H
