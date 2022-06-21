// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_POLICIES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_POLICIES_H

#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
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

class MockDataRetryPolicy : public bigtable::DataRetryPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<bigtable::DataRetryPolicy>, clone, (),
              (const, override));
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(void, OnFailureImpl, (), (override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
};

class MockIdempotentMutationPolicy : public bigtable::IdempotentMutationPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<bigtable::IdempotentMutationPolicy>, clone, (),
              (const, override));
  MOCK_METHOD(bool, is_idempotent, (google::bigtable::v2::Mutation const&),
              (override));
  MOCK_METHOD(bool, is_idempotent,
              (google::bigtable::v2::CheckAndMutateRowRequest const&),
              (override));
};

class MockRetryPolicy : public RPCRetryPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<RPCRetryPolicy>, clone, (), (const, override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext&), (const, override));
  MOCK_METHOD(bool, OnFailure, (grpc::Status const& status), (override));
  MOCK_METHOD(bool, OnFailure, (Status const& status), (override));
};

class MockBackoffPolicy : public RPCBackoffPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<RPCBackoffPolicy>, clone, (), (const, override));
  MOCK_METHOD(void, Setup, (grpc::ClientContext&), (const, override));
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion, (Status const&),
              (override));
  MOCK_METHOD(std::chrono::milliseconds, OnCompletion, (grpc::Status const&),
              (override));
};

class MockPollingPolicy : public PollingPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<PollingPolicy>, clone, (), (const));
  MOCK_METHOD(void, Setup, (grpc::ClientContext&), (override));
  MOCK_METHOD(bool, IsPermanentError, (Status const&), (override));
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(bool, Exhausted, (), (override));
  MOCK_METHOD(std::chrono::milliseconds, WaitPeriod, (), (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_POLICIES_H
