// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_stub.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <grpcpp/impl/codegen/status_code_enum.h>
#include <deque>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::AsyncGrpcOperation;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::_;
using ::testing::Optional;
using ::testing::Return;

class MockGrpcGoldenKitchenSinkStub : public ::google::test::admin::database::
                                          v1::GoldenKitchenSink::StubInterface {
 public:
  ~MockGrpcGoldenKitchenSinkStub() override = default;
  MOCK_METHOD(
      ::grpc::Status, GenerateAccessToken,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::google::test::admin::database::v1::GenerateAccessTokenResponse*
           response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GenerateIdToken,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&
           request,
       ::google::test::admin::database::v1::GenerateIdTokenResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>*,
      AsyncGenerateAccessTokenRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>*,
      PrepareAsyncGenerateAccessTokenRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateIdTokenResponse>*,
      AsyncGenerateIdTokenRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateIdTokenResponse>*,
      PrepareAsyncGenerateIdTokenRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, WriteLogEntries,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&
           request,
       ::google::test::admin::database::v1::WriteLogEntriesResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::WriteLogEntriesResponse>*,
      AsyncWriteLogEntriesRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::WriteLogEntriesResponse>*,
      PrepareAsyncWriteLogEntriesRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListLogs,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListLogsRequest const& request,
       ::google::test::admin::database::v1::ListLogsResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListLogsResponse>*,
      AsyncListLogsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListLogsRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListLogsResponse>*,
      PrepareAsyncListLogsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListLogsRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientReaderInterface<Response>*, StreamingReadRaw,
              (::grpc::ClientContext * context, Request const& request),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncReaderInterface<Response>*,
              AsyncStreamingReadRaw,
              (::grpc::ClientContext * context, Request const& request,
               ::grpc::CompletionQueue* cq, void* tag),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncReaderInterface<Response>*,
              PrepareAsyncStreamingReadRaw,
              (::grpc::ClientContext * context, Request const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::Status, Omitted1,
              (::grpc::ClientContext * context,
               ::google::protobuf::Empty const& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncOmitted1Raw,
      (::grpc::ClientContext * context,
       ::google::protobuf::Empty const& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncOmitted1Raw,
      (::grpc::ClientContext * context,
       ::google::protobuf::Empty const& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::Status, Omitted2,
              (::grpc::ClientContext * context,
               ::google::protobuf::Empty const& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncOmitted2Raw,
      (::grpc::ClientContext * context,
       ::google::protobuf::Empty const& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncOmitted2Raw,
      (::grpc::ClientContext * context,
       ::google::protobuf::Empty const& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListServiceAccountKeys,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListServiceAccountKeysRequest const&
           request,
       ::google::test::admin::database::v1::ListServiceAccountKeysResponse*
           response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>*,
      AsyncListServiceAccountKeysRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListServiceAccountKeysRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>*,
      PrepareAsyncListServiceAccountKeysRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListServiceAccountKeysRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::Status, DoNothing,
              (::grpc::ClientContext * context,
               ::google::protobuf::Empty const& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDoNothingRaw,
      (::grpc::ClientContext * context,
       ::google::protobuf::Empty const& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDoNothingRaw,
      (::grpc::ClientContext * context,
       ::google::protobuf::Empty const& request, ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::Status, Deprecated1,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDeprecated1Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDeprecated1Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::Status, Deprecated2,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDeprecated2Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDeprecated2Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));

  using StreamingReadWriteInterface =
      ::grpc::ClientReaderWriterInterface<Request, Response>;
  using StreamingReadWriteAsyncInterface =
      ::grpc::ClientAsyncReaderWriterInterface<Request, Response>;
  MOCK_METHOD(StreamingReadWriteInterface*, StreamingReadWriteRaw,
              (::grpc::ClientContext * context), (override));
  MOCK_METHOD(StreamingReadWriteAsyncInterface*, AsyncStreamingReadWriteRaw,
              (::grpc::ClientContext*, ::grpc::CompletionQueue*, void*),
              (override));
  MOCK_METHOD(StreamingReadWriteAsyncInterface*,
              PrepareAsyncStreamingReadWriteRaw,
              (::grpc::ClientContext*, ::grpc::CompletionQueue*), (override));

  MOCK_METHOD((::grpc::ClientWriterInterface<Request>*), StreamingWriteRaw,
              (::grpc::ClientContext*, Response*), (override));
  MOCK_METHOD((::grpc::ClientAsyncWriterInterface<Request>*),
              AsyncStreamingWriteRaw,
              (::grpc::ClientContext*, Response*, ::grpc::CompletionQueue*,
               void*),
              (override));
  MOCK_METHOD((::grpc::ClientAsyncWriterInterface<Request>*),
              PrepareAsyncStreamingWriteRaw,
              (::grpc::ClientContext*, Response*, ::grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      ::grpc::Status, ExplicitRouting1,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&
           request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncExplicitRouting1Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncExplicitRouting1Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ExplicitRouting2,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&
           request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncExplicitRouting2Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncExplicitRouting2Raw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
};

class MockLocationStub
    : public google::cloud::location::Locations::StubInterface {
 public:
  ~MockLocationStub() override = default;
  MOCK_METHOD(::grpc::Status, ListLocations,
              (::grpc::ClientContext * context,
               ::google::cloud::location::ListLocationsRequest const& request,
               ::google::cloud::location::ListLocationsResponse* response),
              (override));

  MOCK_METHOD(::grpc::Status, GetLocation,
              (::grpc::ClientContext * context,
               ::google::cloud::location::GetLocationRequest const& request,
               ::google::cloud::location::Location* response),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::ListLocationsResponse>*,
              AsyncListLocationsRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::ListLocationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::ListLocationsResponse>*,
              PrepareAsyncListLocationsRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::ListLocationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::Location>*,
              AsyncGetLocationRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::GetLocationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::Location>*,
              PrepareAsyncGetLocationRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::GetLocationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
};

class MockIamPolicyStub : public google::iam::v1::IAMPolicy::StubInterface {
 public:
  ~MockIamPolicyStub() override = default;
  MOCK_METHOD(::grpc::Status, SetIamPolicy,
              (::grpc::ClientContext * context,
               ::google::iam::v1::SetIamPolicyRequest const& request,
               ::google::iam::v1::Policy* response),
              (override));

  MOCK_METHOD(::grpc::Status, GetIamPolicy,
              (::grpc::ClientContext * context,
               ::google::iam::v1::GetIamPolicyRequest const& request,
               ::google::iam::v1::Policy* response),
              (override));

  MOCK_METHOD(::grpc::Status, TestIamPermissions,
              (::grpc::ClientContext * context,
               ::google::iam::v1::TestIamPermissionsRequest const& request,
               ::google::iam::v1::TestIamPermissionsResponse* response),
              (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      AsyncSetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::SetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      PrepareAsyncSetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::SetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      AsyncGetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::GetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      PrepareAsyncGetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::GetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>*,
              AsyncTestIamPermissionsRaw,
              (::grpc::ClientContext * context,
               ::google::iam::v1::TestIamPermissionsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>*,
              PrepareAsyncTestIamPermissionsRaw,
              (::grpc::ClientContext * context,
               ::google::iam::v1::TestIamPermissionsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
};

class MockOperationsStub
    : public google::longrunning::Operations::StubInterface {
 public:
  ~MockOperationsStub() override = default;
  MOCK_METHOD(::grpc::Status, ListOperations,
              (::grpc::ClientContext * context,
               ::google::longrunning::ListOperationsRequest const& request,
               ::google::longrunning::ListOperationsResponse* response),
              (override));

  MOCK_METHOD(::grpc::Status, GetOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::GetOperationRequest const& request,
               ::google::longrunning::Operation* response),
              (override));

  MOCK_METHOD(::grpc::Status, DeleteOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::DeleteOperationRequest const& request,
               ::google::protobuf::Empty* response),
              (override));

  MOCK_METHOD(::grpc::Status, CancelOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::CancelOperationRequest const& request,
               ::google::protobuf::Empty* response),
              (override));

  MOCK_METHOD(::grpc::Status, WaitOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::WaitOperationRequest const& request,
               ::google::longrunning::Operation* response),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::ListOperationsResponse>*,
              AsyncListOperationsRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::ListOperationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::ListOperationsResponse>*,
              PrepareAsyncListOperationsRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::ListOperationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncGetOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::GetOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncGetOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::GetOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDeleteOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::DeleteOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDeleteOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::DeleteOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncCancelOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::CancelOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncCancelOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::CancelOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncWaitOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::WaitOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncWaitOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::WaitOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
};

class GoldenKitchenSinkStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    grpc_stub_ = std::make_unique<MockGrpcGoldenKitchenSinkStub>();
    operations_stub_ = std::make_unique<MockOperationsStub>();
    iampolicy_stub_ = std::make_unique<MockIamPolicyStub>();
    location_stub_ = std::make_unique<MockLocationStub>();
  }

  static grpc::Status GrpcTransientError() {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  }
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::unique_ptr<MockGrpcGoldenKitchenSinkStub> grpc_stub_;
  std::unique_ptr<MockOperationsStub> operations_stub_;
  std::unique_ptr<MockIamPolicyStub> iampolicy_stub_;
  std::unique_ptr<MockLocationStub> location_stub_;
};

TEST_F(GoldenKitchenSinkStubTest, GetLocation) {
  grpc::Status status;
  grpc::ClientContext context;
  google::cloud::location::GetLocationRequest request;
  EXPECT_CALL(*location_stub_, GetLocation(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.GetLocation(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetLocation(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, ListOperations) {
  grpc::Status status;
  grpc::ClientContext context;
  google::longrunning::ListOperationsRequest request;
  EXPECT_CALL(*operations_stub_, ListOperations(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.ListOperations(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListOperations(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, GetIamPolicy) {
  grpc::Status status;
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  EXPECT_CALL(*iampolicy_stub_, GetIamPolicy(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.GetIamPolicy(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetIamPolicy(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, GenerateAccessToken) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  EXPECT_CALL(*grpc_stub_, GenerateAccessToken(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.GenerateAccessToken(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GenerateAccessToken(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, GenerateIdToken) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  EXPECT_CALL(*grpc_stub_, GenerateIdToken(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.GenerateIdToken(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GenerateIdToken(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, WriteLogEntries) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  EXPECT_CALL(*grpc_stub_, WriteLogEntries(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.WriteLogEntries(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.WriteLogEntries(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, ListLogs) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  EXPECT_CALL(*grpc_stub_, ListLogs(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.ListLogs(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListLogs(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, ListServiceAccountKeys) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  EXPECT_CALL(*grpc_stub_, ListServiceAccountKeys(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success = stub.ListServiceAccountKeys(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListServiceAccountKeys(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

class MockStreamingReadResponse
    : public ::grpc::ClientReaderInterface<Response> {
 public:
  MOCK_METHOD(::grpc::Status, Finish, (), (override));
  MOCK_METHOD(bool, NextMessageSize, (uint32_t*), (override));
  MOCK_METHOD(bool, Read, (Response*), (override));
  MOCK_METHOD(void, WaitForInitialMetadata, (), (override));
};

TEST_F(GoldenKitchenSinkStubTest, StreamingRead) {
  grpc::Status status;
  auto success_response = std::make_unique<MockStreamingReadResponse>();
  auto failure_response = std::make_unique<MockStreamingReadResponse>();
  EXPECT_CALL(*success_response, Read).WillOnce(Return(false));
  EXPECT_CALL(*success_response, Finish()).WillOnce(Return(status));
  EXPECT_CALL(*failure_response, Read).WillOnce(Return(false));
  EXPECT_CALL(*failure_response, Finish).WillOnce(Return(GrpcTransientError()));

  Request request;
  EXPECT_CALL(*grpc_stub_, StreamingReadRaw)
      .WillOnce(Return(success_response.release()))
      .WillOnce(Return(failure_response.release()));
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto success_stream = stub.StreamingRead(
      std::make_shared<grpc::ClientContext>(), Options{}, request);
  Response response;
  EXPECT_THAT(success_stream->Read(&response), Optional(IsOk()));
  auto failure_stream = stub.StreamingRead(
      std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(failure_stream->Read(&response),
              Optional(StatusIs(StatusCode::kUnavailable)));
}

class MockWriteObjectResponse
    : public ::grpc::ClientWriterInterface<
          google::test::admin::database::v1::Request> {
 public:
  MOCK_METHOD(bool, Write, (Request const&, grpc::WriteOptions), (override));
  MOCK_METHOD(bool, WritesDone, (), (override));
  MOCK_METHOD(::grpc::Status, Finish, (), (override));
};

TEST_F(GoldenKitchenSinkStubTest, StreamingWrite) {
  auto context = std::make_shared<grpc::ClientContext>();
  Request request;
  EXPECT_CALL(*grpc_stub_, StreamingWriteRaw(context.get(), _))
      .WillOnce([](::grpc::ClientContext*, Response*) {
        auto stream = std::make_unique<MockWriteObjectResponse>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(true));
        EXPECT_CALL(*stream, WritesDone).WillOnce(Return(true));
        EXPECT_CALL(*stream, Finish).WillOnce(Return(grpc::Status::OK));
        return stream.release();
      });
  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));
  auto stream = stub.StreamingWrite(std::move(context), Options{});
  EXPECT_TRUE(stream->Write(Request{}, grpc::WriteOptions()));
  EXPECT_THAT(stream->Close(), StatusIs(StatusCode::kOk));
}

class MockAsyncStreamingReadWriteResponse
    : public grpc::ClientAsyncReaderWriterInterface<Request, Response> {
 public:
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, Read, (Response*, void*), (override));
  MOCK_METHOD(void, Write, (Request const&, grpc::WriteOptions, void*),
              (override));
  MOCK_METHOD(void, Write, (Request const&, void*), (override));
  MOCK_METHOD(void, WritesDone, (void*), (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

TEST_F(GoldenKitchenSinkStubTest, AsyncStreamingWriteRead) {
  grpc::Status status;
  EXPECT_CALL(*grpc_stub_, PrepareAsyncStreamingReadWriteRaw)
      .WillOnce([](grpc::ClientContext*, grpc::CompletionQueue*) {
        auto stream = std::make_unique<MockAsyncStreamingReadWriteResponse>();
        EXPECT_CALL(*stream, StartCall).Times(1);
        using ::testing::_;
        EXPECT_CALL(*stream, Write(_, _, _)).Times(1);
        EXPECT_CALL(*stream, WritesDone).Times(1);
        EXPECT_CALL(*stream, Read).Times(2);
        EXPECT_CALL(*stream, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return stream.release();  // gRPC assumes ownership of `stream`.
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(Return(nullptr));

  std::deque<std::shared_ptr<AsyncGrpcOperation>> operations;
  auto notify_next_op = [&](bool ok) {
    auto op = std::move(operations.front());
    operations.pop_front();
    op->Notify(ok);
  };

  EXPECT_CALL(*mock_cq, StartOperation)
      .WillRepeatedly([&operations](std::shared_ptr<AsyncGrpcOperation> op,
                                    absl::FunctionRef<void(void*)> call) {
        void* tag = op.get();
        operations.push_back(std::move(op));
        call(tag);
      });
  google::cloud::CompletionQueue cq(mock_cq);

  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));

  auto stream = stub.AsyncStreamingReadWrite(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}));
  auto start = stream->Start();
  notify_next_op(true);
  EXPECT_TRUE(start.get());

  auto write = stream->Write(Request{}, grpc::WriteOptions{});
  notify_next_op(true);
  EXPECT_TRUE(write.get());

  auto read0 = stream->Read();
  notify_next_op(true);
  EXPECT_TRUE(read0.get().has_value());

  auto read1 = stream->Read();
  notify_next_op(false);
  EXPECT_FALSE(read1.get().has_value());

  auto writes_done = stream->WritesDone();
  notify_next_op(true);
  EXPECT_TRUE(writes_done.get());

  auto finish = stream->Finish();
  notify_next_op(true);
  EXPECT_THAT(finish.get(), IsOk());
}

class MockAsyncStreamingReadResponse
    : public grpc::ClientAsyncReaderInterface<Response> {
 public:
  MOCK_METHOD(void, Read, (Response*, void*), (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

TEST_F(GoldenKitchenSinkStubTest, AsyncStreamingRead) {
  grpc::Status status;
  EXPECT_CALL(*grpc_stub_, PrepareAsyncStreamingReadRaw)
      .WillOnce([](grpc::ClientContext*, Request const&,
                   grpc::CompletionQueue*) {
        auto stream = std::make_unique<MockAsyncStreamingReadResponse>();
        EXPECT_CALL(*stream, StartCall).Times(1);
        EXPECT_CALL(*stream, Read).Times(2);
        EXPECT_CALL(*stream, Finish).WillOnce([](grpc::Status* status, void*) {
          *status = grpc::Status::OK;
        });
        return stream.release();  // gRPC assumes ownership of `stream`.
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(Return(nullptr));

  std::deque<std::shared_ptr<AsyncGrpcOperation>> operations;
  auto notify_next_op = [&](bool ok) {
    auto op = std::move(operations.front());
    operations.pop_front();
    op->Notify(ok);
  };

  EXPECT_CALL(*mock_cq, StartOperation)
      .WillRepeatedly([&operations](std::shared_ptr<AsyncGrpcOperation> op,
                                    absl::FunctionRef<void(void*)> call) {
        void* tag = op.get();
        operations.push_back(std::move(op));
        call(tag);
      });
  google::cloud::CompletionQueue cq(mock_cq);

  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));

  Request request;
  auto stream = stub.AsyncStreamingRead(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  auto start = stream->Start();
  notify_next_op(true);
  EXPECT_TRUE(start.get());

  auto read0 = stream->Read();
  notify_next_op(true);
  EXPECT_TRUE(read0.get().has_value());

  auto read1 = stream->Read();
  notify_next_op(false);
  EXPECT_FALSE(read1.get().has_value());

  auto finish = stream->Finish();
  notify_next_op(true);
  EXPECT_THAT(finish.get(), IsOk());
}

class MockAsyncStreamingWriteResponse
    : public grpc::ClientAsyncWriterInterface<Request> {
 public:
  MOCK_METHOD(void, StartCall, (void*), (override));
  MOCK_METHOD(void, Write, (Request const&, grpc::WriteOptions, void*),
              (override));
  MOCK_METHOD(void, Write, (Request const&, void*), (override));
  MOCK_METHOD(void, WritesDone, (void*), (override));
  MOCK_METHOD(void, Finish, (grpc::Status*, void*), (override));
  MOCK_METHOD(void, ReadInitialMetadata, (void*), (override));
};

TEST_F(GoldenKitchenSinkStubTest, AsyncStreamingWrite) {
  grpc::Status status;
  EXPECT_CALL(*grpc_stub_, PrepareAsyncStreamingWriteRaw)
      .WillOnce(
          [](grpc::ClientContext*, Response* response, grpc::CompletionQueue*) {
            auto stream = std::make_unique<MockAsyncStreamingWriteResponse>();
            EXPECT_CALL(*stream, StartCall).Times(1);
            using ::testing::_;
            EXPECT_CALL(*stream, Write(_, _, _)).Times(1);
            EXPECT_CALL(*stream, WritesDone).Times(1);
            EXPECT_CALL(*stream, Finish)
                .WillOnce([response](grpc::Status* status, void*) {
                  response->set_response("Finish()");
                  *status = grpc::Status::OK;
                });
            return stream.release();  // gRPC assumes ownership of `stream`.
          });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, cq).WillRepeatedly(Return(nullptr));

  std::deque<std::shared_ptr<AsyncGrpcOperation>> operations;
  auto notify_next_op = [&](bool ok) {
    auto op = std::move(operations.front());
    operations.pop_front();
    op->Notify(ok);
  };

  EXPECT_CALL(*mock_cq, StartOperation)
      .WillRepeatedly([&operations](std::shared_ptr<AsyncGrpcOperation> op,
                                    absl::FunctionRef<void(void*)> call) {
        void* tag = op.get();
        operations.push_back(std::move(op));
        call(tag);
      });
  google::cloud::CompletionQueue cq(mock_cq);

  DefaultGoldenKitchenSinkStub stub(
      std::move(grpc_stub_), std::move(operations_stub_),
      std::move(iampolicy_stub_), std::move(location_stub_));

  auto stream = stub.AsyncStreamingWrite(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}));
  auto start = stream->Start();
  notify_next_op(true);
  EXPECT_TRUE(start.get());

  auto write = stream->Write(Request{}, grpc::WriteOptions());
  notify_next_op(true);
  EXPECT_TRUE(write.get());

  auto writes_done = stream->WritesDone();
  notify_next_op(false);
  EXPECT_FALSE(writes_done.get());

  auto pending_response = stream->Finish();
  notify_next_op(true);
  auto response = pending_response.get();
  ASSERT_THAT(response, IsOk());
  EXPECT_EQ(response->response(), "Finish()");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
