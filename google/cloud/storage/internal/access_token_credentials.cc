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

#include "google/cloud/storage/internal/access_token_credentials.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

auto constexpr kExpirationSlack = std::chrono::minutes(5);

StatusOr<std::string> AccessTokenCredentials::AuthorizationHeader() {
  std::unique_lock<std::mutex> lk(mu_);
  auto const deadline = std::chrono::system_clock::now() + kExpirationSlack;
  cv_.wait(lk, [this] { return !refreshing_; });
  if (deadline < expiration_) return header_;
  // The access token has expired, or is about to expire, refresh it.
  // Avoid deadlocks by releasing the lock before calling any external function.
  refreshing_ = true;
  lk.unlock();
  auto refresh = source_();
  lk.lock();
  refreshing_ = false;
  token_ = std::move(refresh.token);
  expiration_ = refresh.expiration;
  header_ = "Authorization: Bearer " + token_;
  cv_.notify_all();
  return header_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
