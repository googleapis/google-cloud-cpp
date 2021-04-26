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

#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/grpc_access_token_authentication.h"
#include "google/cloud/internal/grpc_channel_credentials_authentication.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::unique_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<Credentials> const& credentials) {
  struct Visitor : public CredentialsVisitor {
    std::unique_ptr<GrpcAuthenticationStrategy> result;

    void visit(GoogleDefaultCredentialsConfig&) override {
      result = absl::make_unique<GrpcChannelCredentialsAuthentication>(
          grpc::GoogleDefaultCredentials());
    }
    void visit(AccessTokenConfig& cfg) override {
      result =
          absl::make_unique<GrpcAccessTokenAuthentication>(cfg.access_token());
    }
  } visitor;

  CredentialsVisitor::dispatch(*credentials, visitor);
  return std::move(visitor.result);
}

std::unique_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<grpc::ChannelCredentials> const& credentials) {
  return absl::make_unique<GrpcChannelCredentialsAuthentication>(credentials);
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
