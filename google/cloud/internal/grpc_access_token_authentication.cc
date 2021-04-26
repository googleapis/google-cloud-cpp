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

#include "google/cloud/internal/grpc_access_token_authentication.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::shared_ptr<grpc::Channel> GrpcAccessTokenAuthentication::CreateChannel(
    std::string const& endpoint, grpc::ChannelArguments const& arguments) {
  // TODO(#6311) - support setting SSL options
  auto credentials = grpc::SslCredentials(grpc::SslCredentialsOptions{});
  return grpc::CreateCustomChannel(endpoint, credentials, arguments);
}

Status GrpcAccessTokenAuthentication::ConfigureContext(
    grpc::ClientContext& context) {
  context.set_credentials(credentials_);
  return Status{};
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
