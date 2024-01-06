// Copyright 2021 Google LLC
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

#include "google/cloud/internal/grpc_access_token_authentication.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

GrpcAccessTokenAuthentication::GrpcAccessTokenAuthentication(
    AccessToken const& access_token, Options const& opts)
    : credentials_(grpc::AccessTokenCredentials(access_token.token)) {
  auto cainfo = LoadCAInfo(opts);
  if (cainfo) ssl_options_.pem_root_certs = std::move(*cainfo);
}

std::shared_ptr<grpc::Channel> GrpcAccessTokenAuthentication::CreateChannel(
    std::string const& endpoint, grpc::ChannelArguments const& arguments) {
  return grpc::CreateCustomChannel(endpoint, grpc::SslCredentials(ssl_options_),
                                   arguments);
}

bool GrpcAccessTokenAuthentication::RequiresConfigureContext() const {
  return true;
}

Status GrpcAccessTokenAuthentication::ConfigureContext(
    grpc::ClientContext& context) {
  context.set_credentials(credentials_);
  return Status{};
}

future<StatusOr<std::shared_ptr<grpc::ClientContext>>>
GrpcAccessTokenAuthentication::AsyncConfigureContext(
    std::shared_ptr<grpc::ClientContext> context) {
  context->set_credentials(credentials_);
  return make_ready_future(make_status_or(std::move(context)));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
