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

#include "google/cloud/internal/grpc_api_key_authentication.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

GrpcApiKeyAuthentication::GrpcApiKeyAuthentication(std::string api_key)
    : api_key_(std::move(api_key)) {}

std::shared_ptr<grpc::Channel> GrpcApiKeyAuthentication::CreateChannel(
    std::string const& endpoint, grpc::ChannelArguments const& arguments) {
  return grpc::CreateCustomChannel(endpoint, grpc::SslCredentials({}),
                                   arguments);
}

bool GrpcApiKeyAuthentication::RequiresConfigureContext() const { return true; }

Status GrpcApiKeyAuthentication::ConfigureContext(
    grpc::ClientContext& context) {
  context.AddMetadata("x-goog-api-key", api_key_);
  return Status{};
}

future<StatusOr<std::shared_ptr<grpc::ClientContext>>>
GrpcApiKeyAuthentication::AsyncConfigureContext(
    std::shared_ptr<grpc::ClientContext> context) {
  context->AddMetadata("x-goog-api-key", api_key_);
  return make_ready_future(make_status_or(std::move(context)));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
