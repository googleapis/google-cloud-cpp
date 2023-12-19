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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_GRPC_AUTHENTICATION_STRATEGY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_GRPC_AUTHENTICATION_STRATEGY_H

#include "google/cloud/internal/unified_grpc_credentials.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

class MockAuthenticationStrategy
    : public ::google::cloud::internal::GrpcAuthenticationStrategy {
 public:
  MOCK_METHOD(std::shared_ptr<grpc::Channel>, CreateChannel,
              (std::string const&, grpc::ChannelArguments const&), (override));
  MOCK_METHOD(bool, RequiresConfigureContext, (), (const, override));
  MOCK_METHOD(Status, ConfigureContext, (grpc::ClientContext&), (override));
  MOCK_METHOD(future<StatusOr<std::shared_ptr<grpc::ClientContext>>>,
              AsyncConfigureContext, (std::shared_ptr<grpc::ClientContext>),
              (override));
};

/**
 * Create and set expectations on a mock authentication strategy.
 *
 * Many of our tests initialize a MockAuthenticationStrategy and set up the same
 * expectations, namely that the test will use the strategy twice, and the
 * first time it will fail with `kInvalidArgument`, while the second time
 * will set the call credentials in the client context.
 *
 * This function refactors that set up so we don't have to cut&paste it in too
 * many tests.
 */
std::shared_ptr<MockAuthenticationStrategy> MakeTypicalMockAuth();

/**
 * Create and set expectations on a mock authentication strategy.
 *
 * Like MakeTypicalMockAuth() but set the expectations for an asynchronous
 * request.
 */
std::shared_ptr<MockAuthenticationStrategy> MakeTypicalAsyncMockAuth();

/**
 * Create a mock authentication strategy with inoffensive default behavior.
 *
 * This is useful for testing the stub factory interfaces. If asked, it will
 * create a channel that is not null.
 */
std::shared_ptr<MockAuthenticationStrategy> MakeStubFactoryMockAuth();

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_GRPC_AUTHENTICATION_STRATEGY_H
