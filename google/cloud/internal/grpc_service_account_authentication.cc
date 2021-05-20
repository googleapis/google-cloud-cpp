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

#include "google/cloud/internal/grpc_service_account_authentication.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

GrpcServiceAccountAuthentication::GrpcServiceAccountAuthentication(
    std::string const& json_object, Options const& opts)
    : credentials_(grpc::ServiceAccountJWTAccessCredentials(json_object)) {
  auto cainfo = LoadCAInfo(opts);
  if (cainfo) ssl_options_.pem_root_certs = std::move(*cainfo);
}

std::shared_ptr<grpc::Channel> GrpcServiceAccountAuthentication::CreateChannel(
    std::string const& endpoint, grpc::ChannelArguments const& arguments) {
  // TODO(#6311) - support setting SSL options
  auto credentials = grpc::SslCredentials(grpc::SslCredentialsOptions{});
  return grpc::CreateCustomChannel(endpoint, credentials, arguments);
}

bool GrpcServiceAccountAuthentication::RequiresConfigureContext() const {
  return true;
}

Status GrpcServiceAccountAuthentication::ConfigureContext(
    grpc::ClientContext& context) {
  context.set_credentials(credentials_);
  return Status{};
}

future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
GrpcServiceAccountAuthentication::AsyncConfigureContext(
    std::unique_ptr<grpc::ClientContext> context) {
  context->set_credentials(credentials_);
  return make_ready_future(make_status_or(std::move(context)));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
