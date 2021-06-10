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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_GRPC_AUTHENTICATION_STRATEGY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_GRPC_AUTHENTICATION_STRATEGY_H

#include "google/cloud/internal/unified_grpc_credentials.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

class MockAuthenticationStrategy
    : public ::google::cloud::internal::GrpcAuthenticationStrategy {
 public:
  MOCK_METHOD(std::shared_ptr<grpc::Channel>, CreateChannel,
              (std::string const&, grpc::ChannelArguments const&), (override));
  MOCK_METHOD(bool, RequiresConfigureContext, (), (const, override));
  MOCK_METHOD(Status, ConfigureContext, (grpc::ClientContext&), (override));
  MOCK_METHOD(future<StatusOr<std::unique_ptr<grpc::ClientContext>>>,
              AsyncConfigureContext, (std::unique_ptr<grpc::ClientContext>),
              (override));
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_MOCK_GRPC_AUTHENTICATION_STRATEGY_H
