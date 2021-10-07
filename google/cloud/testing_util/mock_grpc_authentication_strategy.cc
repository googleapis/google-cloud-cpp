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

#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

std::shared_ptr<MockAuthenticationStrategy> MakeTypicalMockAuth() {
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, ConfigureContext)
      .WillOnce([](grpc::ClientContext&) {
        return Status(StatusCode::kInvalidArgument, "cannot-set-credentials");
      })
      .WillOnce([](grpc::ClientContext& context) {
        context.set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return Status{};
      });
  return auth;
}

std::shared_ptr<MockAuthenticationStrategy> MakeTypicalAsyncMockAuth() {
  using ReturnType = StatusOr<std::unique_ptr<grpc::ClientContext>>;
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, AsyncConfigureContext)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        return make_ready_future(ReturnType(
            Status(StatusCode::kInvalidArgument, "cannot-set-credentials")));
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context) {
        context->set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return make_ready_future(make_status_or(std::move(context)));
      });
  return auth;
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
