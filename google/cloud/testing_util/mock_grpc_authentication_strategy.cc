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

#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/internal/make_status.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

using ::testing::NiceMock;
using ::testing::Return;

std::shared_ptr<MockAuthenticationStrategy> MakeTypicalMockAuth() {
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, ConfigureContext)
      .WillOnce([](grpc::ClientContext&) {
        return internal::InvalidArgumentError("cannot-set-credentials", GCP_ERROR_INFO());
      })
      .WillOnce([](grpc::ClientContext& context) {
        context.set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return Status{};
      });
  return auth;
}

std::shared_ptr<MockAuthenticationStrategy> MakeTypicalAsyncMockAuth() {
  using ReturnType = StatusOr<std::shared_ptr<grpc::ClientContext>>;
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, AsyncConfigureContext)
      .WillOnce([](auto) {
        return make_ready_future(ReturnType(
            internal::InvalidArgumentError("cannot-set-credentials", GCP_ERROR_INFO())));
      })
      .WillOnce([](auto context) {
        context->set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return make_ready_future(make_status_or(std::move(context)));
      });
  return auth;
}

std::shared_ptr<MockAuthenticationStrategy> MakeStubFactoryMockAuth() {
  auto auth = std::make_shared<NiceMock<MockAuthenticationStrategy>>();
  ON_CALL(*auth, CreateChannel)
      .WillByDefault(Return(grpc::CreateCustomChannel(
          "error:///", grpc::InsecureChannelCredentials(), {})));
  ON_CALL(*auth, RequiresConfigureContext).WillByDefault(Return(false));
  return auth;
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
