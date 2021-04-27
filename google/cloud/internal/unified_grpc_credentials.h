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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UNIFIED_GRPC_CREDENTIALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UNIFIED_GRPC_CREDENTIALS_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/credentials.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

class GrpcAuthenticationStrategy {
 public:
  virtual ~GrpcAuthenticationStrategy() = default;

  virtual std::shared_ptr<grpc::Channel> CreateChannel(
      std::string const& endpoint, grpc::ChannelArguments const& arguments) = 0;
  virtual bool RequiresConfigureContext() const = 0;
  virtual Status ConfigureContext(grpc::ClientContext& context) = 0;
  virtual future<StatusOr<std::unique_ptr<grpc::ClientContext>>>
  AsyncConfigureContext(std::unique_ptr<grpc::ClientContext> context) = 0;
};

std::unique_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<Credentials> const& credentials);

std::unique_ptr<GrpcAuthenticationStrategy> CreateAuthenticationStrategy(
    std::shared_ptr<grpc::ChannelCredentials> const& credentials);

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_UNIFIED_GRPC_CREDENTIALS_H
