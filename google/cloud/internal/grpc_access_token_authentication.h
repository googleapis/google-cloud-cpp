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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_ACCESS_TOKEN_AUTHENTICATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_ACCESS_TOKEN_AUTHENTICATION_H

#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

class GrpcAccessTokenAuthentication : public GrpcAuthenticationStrategy {
 public:
  explicit GrpcAccessTokenAuthentication(AccessToken const& access_token,
                                         Options const& opts);
  ~GrpcAccessTokenAuthentication() override = default;

  std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const& endpoint,
      grpc::ChannelArguments const& arguments) override;
  bool RequiresConfigureContext() const override;
  Status ConfigureContext(grpc::ClientContext&) override;
  future<StatusOr<std::unique_ptr<grpc::ClientContext>>> AsyncConfigureContext(
      std::unique_ptr<grpc::ClientContext> context) override;

 private:
  std::shared_ptr<grpc::CallCredentials> credentials_;
  grpc::SslCredentialsOptions ssl_options_;
  bool use_insecure_channel_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_GRPC_ACCESS_TOKEN_AUTHENTICATION_H
