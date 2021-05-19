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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_ASYNC_ACCESS_TOKEN_CACHE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_ASYNC_ACCESS_TOKEN_CACHE_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <chrono>
#include <functional>
#include <mutex>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

using AsyncAccessTokenSource =
    std::function<future<StatusOr<AccessToken>>(CompletionQueue&)>;

/**
 * Cache asynchronously created access tokens.
 *
 * This is a helper class to implement service account impersonation for gRPC.
 * Service account impersonation is implemented by querying the IAM Credentials
 * service, which returns a access token (an opaque string) when the
 * impersonation is allowed. These tokens can be cached, so the library does
 * not need to fetch the access token on each RPC.
 *
 * Because we want to support asynchronous RPCs in the libraries, we need to
 * also fetch these access tokens asynchronously, or we would be blocking the
 * application while fetching the token.
 *
 * Splitting this functionality to a separate class (instead of the
 * GrpcAuthenticationStrategy for service account impersonation) makes for
 * easier testing.
 */
class GrpcAsyncAccessTokenCache
    : public std::enable_shared_from_this<GrpcAsyncAccessTokenCache> {
 public:
  static std::shared_ptr<GrpcAsyncAccessTokenCache> Create(
      CompletionQueue cq, AsyncAccessTokenSource source);

  StatusOr<AccessToken> GetAccessToken(
      std::chrono::system_clock::time_point now =
          std::chrono::system_clock::now());
  future<StatusOr<AccessToken>> AsyncGetAccessToken(
      std::chrono::system_clock::time_point now =
          std::chrono::system_clock::now());

 private:
  using WaiterType = promise<StatusOr<AccessToken>>;
  explicit GrpcAsyncAccessTokenCache(CompletionQueue cq,
                                     AsyncAccessTokenSource source);

  StatusOr<AccessToken> Refresh(std::unique_lock<std::mutex> lk);
  future<StatusOr<AccessToken>> AsyncRefresh(std::unique_lock<std::mutex> lk);

  void StartRefresh(std::unique_lock<std::mutex> lk);
  void OnRefresh(future<StatusOr<AccessToken>>);

  std::weak_ptr<GrpcAsyncAccessTokenCache> WeakFromThis() {
    return std::weak_ptr<GrpcAsyncAccessTokenCache>(shared_from_this());
  }

  CompletionQueue cq_;
  AsyncAccessTokenSource source_;
  std::mutex mu_;
  AccessToken token_;
  bool refreshing_ = false;
  future<StatusOr<AccessToken>> pending_;
  std::vector<WaiterType> waiting_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_ASYNC_ACCESS_TOKEN_CACHE_H
