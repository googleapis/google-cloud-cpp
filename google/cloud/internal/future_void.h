// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_VOID_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_VOID_H_
/**
 * @file
 *
 * Fully specialize `future<void>` and `promise<R>` for void.
 */

#include "google/cloud/internal/port_platform.h"

// C++ futures only make sense when exceptions are enabled.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#include "google/cloud/internal/future_fwd.h"
#include "google/cloud/internal/future_impl.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Implement ISO/IEC TS 19571:2016 future for void.
 */
template <>
class future<void> {
 public:
  future() noexcept : shared_state_() {}
  future(future<void>&& f) noexcept
      : shared_state_(std::move(f.shared_state_)) {}

  // TODO(#1345) - implement the unwrapping constructor.
  // future(future<future<void>>&& f) noexcept;
  // future& operator=(future<future<void>>&& f) noexcept;
  future(future<void> const&) = delete;

  future& operator=(future<void>&& f) noexcept {
    future tmp(std::move(f));
    std::swap(tmp.shared_state_, shared_state_);
    return *this;
  }
  future& operator=(future const&) = delete;

  void get() {
    check_valid();
    std::shared_ptr<shared_state_type> tmp;
    tmp.swap(shared_state_);
    return tmp->get();
  }

  bool valid() const noexcept { return static_cast<bool>(shared_state_); }
  void wait() const {
    check_valid();
    shared_state_->wait();
  }

  template <typename Rep, typename Period>
  std::future_status wait_for(
      std::chrono::duration<Rep, Period> const& rel_time) const {
    check_valid();
    return shared_state_->wait_for(rel_time);
  }

  template <typename Rep, typename Period>
  std::future_status wait_until(
      std::chrono::time_point<Rep, Period> const& abs_time) const {
    check_valid();
    return shared_state_->wait_until(abs_time);
  }

 private:
  /// Shorthand to refer to the shared state type.
  using shared_state_type = internal::future_shared_state<void>;

  friend class promise<void>;
  explicit future(std::shared_ptr<shared_state_type> state)
      : shared_state_(std::move(state)) {}

  void check_valid() const {
    if (not shared_state_) {
      throw std::future_error(std::future_errc::no_state);
    }
  }

  std::shared_ptr<shared_state_type> shared_state_;
};

/**
 * Specialize promise as defined in ISO/IEC TS 19571:2016 for void.
 */
template <>
class promise<void> {
 public:
  /// Create a promise
  promise() : shared_state_(new shared_state) {}

  promise(promise&& rhs) noexcept
      : shared_state_(std::move(rhs.shared_state_)) {
    rhs.shared_state_.reset();
  }

  promise(promise const&) = delete;

  ~promise() {
    if (shared_state_) {
      shared_state_->abandon();
    }
  }

  promise& operator=(promise&& rhs) noexcept {
    promise tmp(std::move(rhs));
    this->swap(tmp);
    return *this;
  }

  promise& operator=(promise const&) = delete;

  void swap(promise& other) noexcept {
    std::swap(shared_state_, other.shared_state_);
  }

  future<void> get_future() {
    shared_state::mark_retrieved(shared_state_);
    return future<void>(shared_state_);
  }

  void set_value() {
    if (not shared_state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    shared_state_->set_value();
  }

  void set_exception(std::exception_ptr p) {
    if (not shared_state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    shared_state_->set_exception(std::move(p));
  }

 private:
  /// Shorthand to refer to the shared state type.
  using shared_state = internal::future_shared_state<void>;
  std::shared_ptr<shared_state> shared_state_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_VOID_H_
