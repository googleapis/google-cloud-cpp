// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_FUTURES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_FUTURES_H

#include "google/cloud/future.h"

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename T>
struct ChainFutureImpl {
  template <typename U>
  // Note: this drops any exceptions contained in `future<U>`
  future<T> operator()(future<U>) {
    return std::move(f);
  }

  future<T> f;
};

/**
 * A helper to capture-by-move futures into a second future continuation.
 *
 * Given two futures `future<U> r` and `future<T> f` we often want to write:
 * @code
 * f.then([tmp = std::move(f)](future<T>) mutable { return std::move(tmp); }
 * @encode
 *
 * Unfortunately we cannot, as the project needs to support C++11. This is
 * a helper to avoid repetition of this pattern.
 */
template <typename T>
ChainFutureImpl<T> ChainFuture(future<T> f) {
  return ChainFutureImpl<T>{std::move(f)};
}

// A RAII helper for ensuring continuations don't run while a mutex is held.
//
// Usable like:
//
// AsyncRoot root;
// std::lock_guard<std::mutex> g{mu_};
// // `then` continuations will not run while the mutex is held.
// return root.get_future().then([this](future<void>){
//     std::lock_guard<std::mutex> g{mu_};
//     DoThing();
// }).then(...);
class AsyncRoot {
 public:
  ~AsyncRoot() { root_.set_value(); }
  // Call at most once.
  future<void> get_future() { return root_.get_future(); }

 private:
  promise<void> root_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_FUTURES_H
