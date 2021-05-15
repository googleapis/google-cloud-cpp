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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BACKOFF_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BACKOFF_POLICY_H

#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/status.h"
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>
#include <grpcpp/impl/codegen/status.h>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockBackoffPolicy : public RPCBackoffPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<RPCBackoffPolicy>, clone, (), (const, override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext & context), (const, override));
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion, (Status const& status),
              (override));
  // TODO(#2344) - remove ::grpc::Status version.
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion,
              (grpc::Status const& status), (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_BACKOFF_POLICY_H
