// Copyright 2021 Google LLC
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

#include "google/cloud/internal/grpc_async_access_token_cache.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

auto constexpr kUseSlack = std::chrono::seconds(30);
auto constexpr kRefreshSlack = std::chrono::minutes(5);

std::shared_ptr<GrpcAsyncAccessTokenCache> GrpcAsyncAccessTokenCache::Create(
    CompletionQueue cq, AsyncAccessTokenSource source) {
  return std::shared_ptr<GrpcAsyncAccessTokenCache>(
      new GrpcAsyncAccessTokenCache(std::move(cq), std::move(source)));
}

StatusOr<AccessToken> GrpcAsyncAccessTokenCache::GetAccessToken(
    std::chrono::system_clock::time_point now) {
  std::unique_lock<std::mutex> lk(mu_);
  if (now + kUseSlack > token_.expiration) return Refresh(std::move(lk));
  auto tmp = token_;
  if (now + kRefreshSlack >= token_.expiration) StartRefresh(std::move(lk));
  return tmp;
}

future<StatusOr<AccessToken>> GrpcAsyncAccessTokenCache::AsyncGetAccessToken(
    std::chrono::system_clock::time_point now) {
  std::unique_lock<std::mutex> lk(mu_);
  if (now + kUseSlack > token_.expiration) return AsyncRefresh(std::move(lk));
  auto tmp = token_;
  if (now + kRefreshSlack >= token_.expiration) StartRefresh(std::move(lk));
  return make_ready_future(make_status_or(tmp));
}

GrpcAsyncAccessTokenCache::GrpcAsyncAccessTokenCache(
    CompletionQueue cq, AsyncAccessTokenSource source)
    : cq_(std::move(cq)), source_(std::move(source)) {}

StatusOr<AccessToken> GrpcAsyncAccessTokenCache::Refresh(
    std::unique_lock<std::mutex> lk) {
  return AsyncRefresh(std::move(lk)).get();
}

future<StatusOr<AccessToken>> GrpcAsyncAccessTokenCache::AsyncRefresh(
    std::unique_lock<std::mutex> lk) {
  waiting_.emplace_back();
  auto result = waiting_.back().get_future();
  StartRefresh(std::move(lk));
  return result;
}

void GrpcAsyncAccessTokenCache::StartRefresh(std::unique_lock<std::mutex> lk) {
  if (refreshing_) return;
  refreshing_ = true;
  auto w = WeakFromThis();
  lk.unlock();
  source_(cq_).then([w](future<StatusOr<AccessToken>> f) {
    if (auto self = w.lock()) self->OnRefresh(std::move(f));
  });
}

void GrpcAsyncAccessTokenCache::OnRefresh(future<StatusOr<AccessToken>> f) {
  std::unique_lock<std::mutex> lk(mu_);
  refreshing_ = false;
  std::vector<WaiterType> waiting;
  waiting.swap(waiting_);
  auto result = f.get();
  StatusOr<AccessToken> value;
  if (result) {
    token_ = *std::move(result);
    value = token_;
  } else {
    value = std::move(result).status();
  }
  // Run the waiters asynchronously to avoid blocking
  struct SetStatus {
    WaiterType p;
    StatusOr<AccessToken> value;
    void operator()() { p.set_value(std::move(value)); }
  };
  for (auto& p : waiting) cq_.RunAsync(SetStatus{std::move(p), value});
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
