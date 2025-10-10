// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_stub.h"
#include "google/cloud/mocks/mock_async_streaming_read_write_rpc.h"
#include "google/cloud/testing_util/mock_async_streaming_read_rpc.h"
#include "google/cloud/testing_util/mock_async_streaming_write_rpc.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockGoldenKitchenSinkStub : public GoldenKitchenSinkStub {
 public:
  ~MockGoldenKitchenSinkStub() override = default;

  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::ListLogsResponse>,
              ListLogs,
              (grpc::ClientContext&, Options const&,
               ::google::test::admin::database::v1::ListLogsRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>,
      ListServiceAccountKeys,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::
           ListServiceAccountKeysRequest const&),
      (override));
  MOCK_METHOD(Status, DoNothing,
              (grpc::ClientContext&, Options const&,
               ::google::protobuf::Empty const&),
              (override));
  MOCK_METHOD(
      Status, Deprecated2,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&),
      (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
                   ::google::test::admin::database::v1::Request,
                   ::google::test::admin::database::v1::Response>>),
              AsyncStreamingReadWrite,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions),
              (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::internal::StreamingWriteRpc<
                   ::google::test::admin::database::v1::Request,
                   ::google::test::admin::database::v1::Response>>),
              StreamingWrite,
              (std::shared_ptr<grpc::ClientContext>, Options const&),
              (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::internal::StreamingReadRpc<
                   ::google::test::admin::database::v1::Response>>),
              StreamingRead,
              (std::shared_ptr<grpc::ClientContext>, Options const&,
               ::google::test::admin::database::v1::Request const&),
              (override));

  MOCK_METHOD(
      Status, ExplicitRouting1,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&),
      (override));

  MOCK_METHOD(
      Status, ExplicitRouting2,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&),
      (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
                   ::google::test::admin::database::v1::Response>>),
              AsyncStreamingRead,
              (google::cloud::CompletionQueue const&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::test::admin::database::v1::Request const&),
              (override));

  MOCK_METHOD(
      (std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
           ::google::test::admin::database::v1::Request,
           ::google::test::admin::database::v1::Response>>),
      AsyncStreamingWrite,
      (google::cloud::CompletionQueue const& cq,
       std::shared_ptr<grpc::ClientContext> context,
       google::cloud::internal::ImmutableOptions options),
      (override));

  MOCK_METHOD(StatusOr<::google::cloud::location::Location>, GetLocation,
              (grpc::ClientContext&, Options const&,
               ::google::cloud::location::GetLocationRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&, Options const&,
               ::google::iam::v1::GetIamPolicyRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::longrunning::ListOperationsResponse>,
              ListOperations,
              (grpc::ClientContext&, Options const&,
               ::google::longrunning::ListOperationsRequest const&),
              (override));
};

class MockStreamingReadRpc
    : public google::cloud::internal::StreamingReadRpc<
          ::google::test::admin::database::v1::Response> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD((absl::optional<Status>), Read,
              (::google::test::admin::database::v1::Response*), (override));
  MOCK_METHOD(RpcMetadata, GetRequestMetadata, (), (const, override));
};

class MockStreamingWriteRpc
    : public internal::StreamingWriteRpc<
          ::google::test::admin::database::v1::Request,
          ::google::test::admin::database::v1::Response> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(bool, Write,
              (::google::test::admin::database::v1::Request const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Response>, Close,
              (), (override));
  MOCK_METHOD(RpcMetadata, GetRequestMetadata, (), (const, override));
};

using MockAsyncStreamingReadWriteRpc =
    google::cloud::mocks::MockAsyncStreamingReadWriteRpc<
        ::google::test::admin::database::v1::Request,
        ::google::test::admin::database::v1::Response>;

using MockAsyncStreamingReadRpc =
    google::cloud::testing_util::MockAsyncStreamingReadRpc<
        google::test::admin::database::v1::Response>;

using MockAsyncStreamingWriteRpc =
    google::cloud::testing_util::MockAsyncStreamingWriteRpc<
        google::test::admin::database::v1::Request,
        google::test::admin::database::v1::Response>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H
