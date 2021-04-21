// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <grpcpp/impl/codegen/status_code_enum.h>
#include <memory>

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {
class MockGrpcGoldenKitchenSinkStub : public ::google::test::admin::database::
                                          v1::GoldenKitchenSink::StubInterface {
 public:
  ~MockGrpcGoldenKitchenSinkStub() override = default;
  MOCK_METHOD(
      ::grpc::Status, GenerateAccessToken,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateAccessTokenRequest&
           request,
       ::google::test::admin::database::v1::GenerateAccessTokenResponse*
           response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GenerateIdToken,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateIdTokenRequest&
           request,
       ::google::test::admin::database::v1::GenerateIdTokenResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>*,
      AsyncGenerateAccessTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateAccessTokenRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>*,
      PrepareAsyncGenerateAccessTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateAccessTokenRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateIdTokenResponse>*,
      AsyncGenerateIdTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateIdTokenRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::GenerateIdTokenResponse>*,
      PrepareAsyncGenerateIdTokenRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GenerateIdTokenRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, WriteLogEntries,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::WriteLogEntriesRequest&
           request,
       ::google::test::admin::database::v1::WriteLogEntriesResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::WriteLogEntriesResponse>*,
      AsyncWriteLogEntriesRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::WriteLogEntriesRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::WriteLogEntriesResponse>*,
      PrepareAsyncWriteLogEntriesRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::WriteLogEntriesRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListLogs,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListLogsRequest& request,
       ::google::test::admin::database::v1::ListLogsResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListLogsResponse>*,
      AsyncListLogsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListLogsRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListLogsResponse>*,
      PrepareAsyncListLogsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListLogsRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientReaderInterface<
                  ::google::test::admin::database::v1::TailLogEntriesResponse>*,
              TailLogEntriesRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::TailLogEntriesRequest&
                   request),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncReaderInterface<
                  ::google::test::admin::database::v1::TailLogEntriesResponse>*,
              AsyncTailLogEntriesRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::TailLogEntriesRequest&
                   request,
               ::grpc::CompletionQueue* cq, void* tag),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncReaderInterface<
                  ::google::test::admin::database::v1::TailLogEntriesResponse>*,
              PrepareAsyncTailLogEntriesRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::TailLogEntriesRequest&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::Status, Omitted1,
              (::grpc::ClientContext * context,
               const ::google::protobuf::Empty& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      AsyncOmitted1Raw,
      (::grpc::ClientContext * context,
       const ::google::protobuf::Empty& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      PrepareAsyncOmitted1Raw,
      (::grpc::ClientContext * context,
       const ::google::protobuf::Empty& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::Status, Omitted2,
              (::grpc::ClientContext * context,
               const ::google::protobuf::Empty& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      AsyncOmitted2Raw,
      (::grpc::ClientContext * context,
       const ::google::protobuf::Empty& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      PrepareAsyncOmitted2Raw,
      (::grpc::ClientContext * context,
       const ::google::protobuf::Empty& request, ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListServiceAccountKeys,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListServiceAccountKeysRequest&
           request,
       ::google::test::admin::database::v1::ListServiceAccountKeysResponse*
           response),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>*,
      AsyncListServiceAccountKeysRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListServiceAccountKeysRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>*,
      PrepareAsyncListServiceAccountKeysRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListServiceAccountKeysRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
};

class GoldenKitchenSinkStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    grpc_stub_ = std::unique_ptr<MockGrpcGoldenKitchenSinkStub>(
        new MockGrpcGoldenKitchenSinkStub());
  }

  static grpc::Status GrpcTransientError() {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  }
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::unique_ptr<MockGrpcGoldenKitchenSinkStub> grpc_stub_;
};

TEST_F(GoldenKitchenSinkStubTest, GenerateAccessToken) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  EXPECT_CALL(*grpc_stub_, GenerateAccessToken(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(std::move(grpc_stub_));
  auto success = stub.GenerateAccessToken(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GenerateAccessToken(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, GenerateIdToken) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  EXPECT_CALL(*grpc_stub_, GenerateIdToken(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(std::move(grpc_stub_));
  auto success = stub.GenerateIdToken(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GenerateIdToken(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, WriteLogEntries) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  EXPECT_CALL(*grpc_stub_, WriteLogEntries(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(std::move(grpc_stub_));
  auto success = stub.WriteLogEntries(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.WriteLogEntries(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenKitchenSinkStubTest, ListLogs) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  EXPECT_CALL(*grpc_stub_, ListLogs(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(std::move(grpc_stub_));
  auto success = stub.ListLogs(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListLogs(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

class MockTailLogEntriesResponse
    : public ::grpc::ClientReaderInterface<
          google::test::admin::database::v1::TailLogEntriesResponse> {
 public:
  MOCK_METHOD(::grpc::Status, Finish, (), (override));
  MOCK_METHOD(bool, NextMessageSize, (uint32_t*), (override));
  MOCK_METHOD(bool, Read,
              (google::test::admin::database::v1::TailLogEntriesResponse*),
              (override));
  MOCK_METHOD(void, WaitForInitialMetadata, (), (override));
};

TEST_F(GoldenKitchenSinkStubTest, TailLogEntries) {
  grpc::Status status;
  auto* success_response = new MockTailLogEntriesResponse;
  auto* failure_response = new MockTailLogEntriesResponse;
  EXPECT_CALL(*success_response, Read).WillOnce(Return(false));
  EXPECT_CALL(*success_response, Finish()).WillOnce(Return(status));
  EXPECT_CALL(*failure_response, Read).WillOnce(Return(false));
  EXPECT_CALL(*failure_response, Finish).WillOnce(Return(GrpcTransientError()));

  google::test::admin::database::v1::TailLogEntriesRequest request;
  EXPECT_CALL(*grpc_stub_, TailLogEntriesRaw)
      .WillOnce(Return(success_response))
      .WillOnce(Return(failure_response));
  DefaultGoldenKitchenSinkStub stub(std::move(grpc_stub_));
  auto success_stream =
      stub.TailLogEntries(absl::make_unique<grpc::ClientContext>(), request);
  auto success_status = absl::get<Status>(success_stream->Read());
  EXPECT_THAT(success_status, IsOk());
  auto failure_stream =
      stub.TailLogEntries(absl::make_unique<grpc::ClientContext>(), request);
  auto failure_status = absl::get<Status>(failure_stream->Read());
  EXPECT_THAT(failure_status, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GoldenKitchenSinkStubTest, ListServiceAccountKeys) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  EXPECT_CALL(*grpc_stub_, ListServiceAccountKeys(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenKitchenSinkStub stub(std::move(grpc_stub_));
  auto success = stub.ListServiceAccountKeys(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListServiceAccountKeys(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
