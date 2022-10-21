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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockGoldenKitchenSinkStub : public GoldenKitchenSinkStub {
 public:
  ~MockGoldenKitchenSinkStub() override = default;

  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::ListLogsResponse>,
              ListLogs,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::ListLogsRequest const&),
              (override));
  MOCK_METHOD(
      (std::unique_ptr<internal::StreamingReadRpc<
           ::google::test::admin::database::v1::TailLogEntriesResponse>>),
      TailLogEntries,
      (std::unique_ptr<grpc::ClientContext> context,
       ::google::test::admin::database::v1::TailLogEntriesRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListServiceAccountKeysResponse>,
      ListServiceAccountKeys,
      (grpc::ClientContext&, ::google::test::admin::database::v1::
                                 ListServiceAccountKeysRequest const&),
      (override));
  MOCK_METHOD(Status, DoNothing,
              (grpc::ClientContext&, ::google::protobuf::Empty const&),
              (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::AsyncStreamingReadWriteRpc<
                   ::google::test::admin::database::v1::AppendRowsRequest,
                   ::google::test::admin::database::v1::AppendRowsResponse>>),
              AsyncAppendRows,
              (google::cloud::CompletionQueue const& cq,
               std::unique_ptr<grpc::ClientContext> context),
              (override));

  MOCK_METHOD((std::unique_ptr<::google::cloud::internal::StreamingWriteRpc<
                   ::google::test::admin::database::v1::WriteObjectRequest,
                   ::google::test::admin::database::v1::WriteObjectResponse>>),
              WriteObject, (std::unique_ptr<grpc::ClientContext> context),
              (override));

  MOCK_METHOD(
      (std::unique_ptr<::google::cloud::internal::AsyncStreamingReadRpc<
           ::google::test::admin::database::v1::TailLogEntriesResponse>>),
      AsyncTailLogEntries,
      (google::cloud::CompletionQueue const& cq,
       std::unique_ptr<grpc::ClientContext> context,
       ::google::test::admin::database::v1::TailLogEntriesRequest const&),
      (override));

  MOCK_METHOD(
      (std::unique_ptr<::google::cloud::internal::AsyncStreamingWriteRpc<
           ::google::test::admin::database::v1::WriteObjectRequest,
           ::google::test::admin::database::v1::WriteObjectResponse>>),
      AsyncWriteObject,
      (google::cloud::CompletionQueue const& cq,
       std::unique_ptr<grpc::ClientContext> context),
      (override));

  MOCK_METHOD(
      Status, ExplicitRouting1,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&),
      (override));

  MOCK_METHOD(
      Status, ExplicitRouting2,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::ExplicitRoutingRequest const&),
      (override));
};

class MockTailLogEntriesStreamingReadRpc
    : public google::cloud::internal::StreamingReadRpc<
          ::google::test::admin::database::v1::TailLogEntriesResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(
      (absl::variant<
          Status, ::google::test::admin::database::v1::TailLogEntriesResponse>),
      Read, (), (override));
  MOCK_METHOD(internal::StreamingRpcMetadata, GetRequestMetadata, (),
              (const, override));
};

class MockWriteObjectStreamingWriteRpc
    : public internal::StreamingWriteRpc<
          ::google::test::admin::database::v1::WriteObjectRequest,
          ::google::test::admin::database::v1::WriteObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(bool, Write,
              (::google::test::admin::database::v1::WriteObjectRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteObjectResponse>, Close,
      (), (override));
  MOCK_METHOD(internal::StreamingRpcMetadata, GetRequestMetadata, (),
              (const, override));
};

class MockTailLogEntriesAsyncStreamingReadRpc
    : public google::cloud::internal::AsyncStreamingReadRpc<
          ::google::test::admin::database::v1::TailLogEntriesResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<absl::optional<
                  ::google::test::admin::database::v1::TailLogEntriesResponse>>,
              Read, (), (override));
  MOCK_METHOD(future<Status>, Finish, (), (override));
};

class MockWriteObjectAsyncStreamingWriteRpc
    : public internal::AsyncStreamingWriteRpc<
          ::google::test::admin::database::v1::WriteObjectRequest,
          ::google::test::admin::database::v1::WriteObjectResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(future<bool>, Start, (), (override));
  MOCK_METHOD(future<bool>, Write,
              (::google::test::admin::database::v1::WriteObjectRequest const&,
               grpc::WriteOptions),
              (override));
  MOCK_METHOD(future<bool>, WritesDone, (), (override));
  MOCK_METHOD(
      future<
          StatusOr<::google::test::admin::database::v1::WriteObjectResponse>>,
      Finish, (), (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_KITCHEN_SINK_STUB_H
