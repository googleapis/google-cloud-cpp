// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_REQUEST_ID_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_REQUEST_ID_STUB_H

#include "generator/integration_tests/golden/v1/internal/request_id_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockRequestIdServiceStub
    : public google::cloud::golden_v1_internal::RequestIdServiceStub {
 public:
  ~MockRequestIdServiceStub() override = default;

  MOCK_METHOD(StatusOr<google::test::requestid::v1::Foo>, CreateFoo,
              (grpc::ClientContext&, Options const&,
               google::test::requestid::v1::CreateFooRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>, AsyncRenameFoo,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::test::requestid::v1::RenameFooRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::longrunning::Operation>, RenameFoo,
              (grpc::ClientContext & context, Options options,
               google::test::requestid::v1::RenameFooRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::test::requestid::v1::ListFoosResponse>, ListFoos,
              (grpc::ClientContext&, Options const&,
               google::test::requestid::v1::ListFoosRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::test::requestid::v1::Foo>>,
              AsyncCreateFoo,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::test::requestid::v1::CreateFooRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::longrunning::GetOperationRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::longrunning::CancelOperationRequest const&),
              (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_REQUEST_ID_STUB_H
