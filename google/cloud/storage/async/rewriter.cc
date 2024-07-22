// Copyright 2024 Google LLC
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

#include "google/cloud/storage/async/rewriter.h"
#include "google/cloud/internal/make_status.h"
#include <memory>

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

template <typename T>
future<StatusOr<T>> TokenError(internal::ErrorInfoBuilder eib) {
  return make_ready_future(StatusOr<T>(
      internal::InvalidArgumentError("invalid token", std::move(eib))));
}

template <typename T>
future<StatusOr<T>> NullImpl(internal::ErrorInfoBuilder eib) {
  return make_ready_future(
      StatusOr<T>(internal::CancelledError("null impl", std::move(eib))));
}

using IterateResponse =
    std::pair<google::storage::v2::RewriteResponse, AsyncToken>;

}  // namespace

AsyncRewriter::AsyncRewriter(std::shared_ptr<AsyncRewriterConnection> impl)
    : impl_(std::move(impl)) {}

AsyncRewriter::~AsyncRewriter() = default;

future<StatusOr<IterateResponse>> AsyncRewriter::Iterate(AsyncToken token) {
  if (!impl_) return NullImpl<IterateResponse>(GCP_ERROR_INFO());
  auto t = storage_internal::MakeAsyncToken(impl_.get());
  if (token != t) return TokenError<IterateResponse>(GCP_ERROR_INFO());

  return impl_->Iterate().then([t = std::move(t), impl = impl_](auto f) mutable
                               -> StatusOr<IterateResponse> {
    auto r = f.get();
    if (!r) return std::move(r).status();
    auto const done = r->has_resource();
    return std::make_pair(*std::move(r), done ? AsyncToken() : std::move(t));
  });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google
