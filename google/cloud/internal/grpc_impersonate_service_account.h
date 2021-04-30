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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_IMPERSONATE_SERVICE_ACCOUNT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_IMPERSONATE_SERVICE_ACCOUNT_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/grpc_async_access_token_cache.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

class MinimalIamCredentialsStub;

class GrpcImpersonateServiceAccount
    : public GrpcAuthenticationStrategy,
      public std::enable_shared_from_this<GrpcImpersonateServiceAccount> {
 public:
  static std::shared_ptr<GrpcImpersonateServiceAccount> Create(
      CompletionQueue cq, ImpersonateServiceAccountConfig const& config,
      Options const& options = {});
  ~GrpcImpersonateServiceAccount() override;

  std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const& endpoint,
      grpc::ChannelArguments const& arguments) override;
  bool RequiresConfigureContext() const override;
  Status ConfigureContext(grpc::ClientContext&) override;
  future<StatusOr<std::unique_ptr<grpc::ClientContext>>> AsyncConfigureContext(
      std::unique_ptr<grpc::ClientContext>) override;

 private:
  GrpcImpersonateServiceAccount(CompletionQueue cq,
                                ImpersonateServiceAccountConfig const& config,
                                Options const& options);

  std::shared_ptr<grpc::CallCredentials> UpdateCallCredentials(
      std::string token);
  StatusOr<std::unique_ptr<grpc::ClientContext>> OnGetCallCredentials(
      std::unique_ptr<grpc::ClientContext> context,
      StatusOr<AccessToken> result);

  std::weak_ptr<GrpcImpersonateServiceAccount> WeakFromThis() {
    return shared_from_this();
  }

  std::shared_ptr<GrpcAsyncAccessTokenCache> cache_;
  std::mutex mu_;
  std::string access_token_;
  std::shared_ptr<grpc::CallCredentials> credentials_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_IMPERSONATE_SERVICE_ACCOUNT_H
