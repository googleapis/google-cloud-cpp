// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_TOKEN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_TOKEN_H

#include "google/cloud/storage/internal/async/token_impl.h"
#include "google/cloud/version.h"
#include <algorithm>
#include <utility>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Serialize streams of asynchronous operations.
 *
 * Some asynchronous APIs produce streams of results, where (1) each of these
 * results is obtained asynchronously, and (2) on a single stream, only one of
 * these results can be requested at a time.
 *
 * Where both conditions apply the APIs will consume an `AsyncToken` as a
 * parameter, and return a `future<std::pair<ResultT, StatusOr<AsyncToken>>`.
 * When this [`future`] is satisfied, and the `StatusOr<>` contains an value,
 * the caller can invoke the API once again. Before the future is satisfied the
 * application does not have (and cannot create) a valid `AsyncToken` to invoke
 * the API again.
 *
 * [`future`]: @ref google::cloud::future
 */
class AsyncToken {
 public:
  /// Create an invalid async token.
  AsyncToken() noexcept = default;

  /// Move constructor. Invalidates @p rhs.
  AsyncToken(AsyncToken&& rhs) noexcept : AsyncToken() {
    std::swap(rhs.tag_, tag_);
  }

  /// Move assignment. Invalidates @p rhs.
  AsyncToken& operator=(AsyncToken&& rhs) noexcept {
    AsyncToken tmp(std::move(rhs));
    std::swap(tmp.tag_, tag_);
    return *this;
  }

  /// Disable copying, this is a move-only class.
  AsyncToken(AsyncToken const&) = delete;
  /// Disable copying, this is a move-only class.
  AsyncToken& operator=(AsyncToken const&) = delete;

  /// Returns `false` for invalidated instances.
  bool valid() const { return tag_ != nullptr; }

  /// Compare two instances.
  friend bool operator==(AsyncToken const& rhs, AsyncToken const& lhs) {
    return rhs.tag_ == lhs.tag_;
  }
  /// Compare two instances.
  friend bool operator!=(AsyncToken const& rhs, AsyncToken const& lhs) {
    return !(rhs == lhs);
  }

 private:
  explicit AsyncToken(void const* tag) : tag_(tag) {}
  friend AsyncToken storage_internal::MakeAsyncToken(void const*);

  void const* tag_ = nullptr;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_ASYNC_TOKEN_H
