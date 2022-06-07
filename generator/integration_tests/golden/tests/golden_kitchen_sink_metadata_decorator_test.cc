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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_metadata_decorator.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/golden/mocks/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_internal::MockGoldenKitchenSinkStub;
using ::google::cloud::golden_internal::MockTailLogEntriesStreamingReadRpc;
using ::google::cloud::golden_internal::MockWriteObjectStreamingWriteRpc;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::test::admin::database::v1::TailLogEntriesRequest;
using ::google::test::admin::database::v1::TailLogEntriesResponse;
using ::google::test::admin::database::v1::WriteObjectRequest;
using ::google::test::admin::database::v1::WriteObjectResponse;
using ::testing::_;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;

class MetadataDecoratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<MockGoldenKitchenSinkStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  Status IsContextMDValid(grpc::ClientContext& context,
                          std::string const& method) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, google::cloud::internal::ApiClientHeader("generator"));
  }

  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

  std::shared_ptr<MockGoldenKitchenSinkStub> mock_;

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

/// Verify the x-goog-user-project metadata is set.
TEST_F(MetadataDecoratorTest, UserProject) {
  // We do this for a single RPC, we are using some knowledge of the
  // implementation to assert that this is enough.
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, Not(Contains(Pair("x-goog-user-project", _))));
        return TransientError();
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-user-project", "test-user-project")));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  // First try without any UserProjectOption
  {
    internal::OptionsSpan span(Options{});
    grpc::ClientContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, request);
    EXPECT_EQ(TransientError(), status.status());
  }
  // Then try with a UserProjectOption
  {
    internal::OptionsSpan span(
        Options{}.set<UserProjectOption>("test-user-project"));
    grpc::ClientContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, request);
    EXPECT_EQ(TransientError(), status.status());
  }
}

TEST_F(MetadataDecoratorTest, GenerateAccessToken) {
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.test.admin.database.v1.GoldenKitchenSink."
                             "GenerateAccessToken"));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateAccessToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, GenerateIdToken) {
  EXPECT_CALL(*mock_, GenerateIdToken)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           GenerateIdTokenRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.GenerateIdToken"));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateIdToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, WriteLogEntries) {
  EXPECT_CALL(*mock_, WriteLogEntries)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           WriteLogEntriesRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.WriteLogEntries"));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto status = stub.WriteLogEntries(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListLogs) {
  EXPECT_CALL(*mock_, ListLogs)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::test::admin::database::v1::ListLogsRequest const&) {
            EXPECT_STATUS_OK(IsContextMDValid(
                context,
                "google.test.admin.database.v1.GoldenKitchenSink.ListLogs"));
            return TransientError();
          });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my_project");
  auto status = stub.ListLogs(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, TailLogEntries) {
  EXPECT_CALL(*mock_, TailLogEntries)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context,
                       TailLogEntriesRequest const&) {
        auto mock_response =
            absl::make_unique<MockTailLogEntriesStreamingReadRpc>();
        EXPECT_CALL(*mock_response, Read)
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.TailLogEntries"));
        return std::unique_ptr<
            internal::StreamingReadRpc<TailLogEntriesResponse>>(
            std::move(mock_response));
      });
  GoldenKitchenSinkMetadata stub(mock_);
  TailLogEntriesRequest request;
  auto response =
      stub.TailLogEntries(absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(absl::get<Status>(response->Read()), Not(IsOk()));
}

TEST_F(MetadataDecoratorTest, ListServiceAccountKeys) {
  EXPECT_CALL(*mock_, ListServiceAccountKeys)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ListServiceAccountKeysRequest const&) {
        EXPECT_STATUS_OK(
            IsContextMDValid(context,
                             "google.test.admin.database.v1.GoldenKitchenSink."
                             "ListServiceAccountKeys"));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  request.set_name("projects/my-project/serviceAccounts/foo@bar.com");
  auto status = stub.ListServiceAccountKeys(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, WriteObject) {
  EXPECT_CALL(*mock_, WriteObject)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.WriteObject"));

        auto stream = absl::make_unique<MockWriteObjectStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(true))
            .WillOnce(Return(false));
        auto response = WriteObjectResponse{};
        response.set_response("test-only");
        EXPECT_CALL(*stream, Close).WillOnce(Return(make_status_or(response)));
        return stream;
      });

  GoldenKitchenSinkMetadata stub(mock_);
  auto stream = stub.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_TRUE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  auto response = stream->Close();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->response(), "test-only");
}

TEST_F(MetadataDecoratorTest, AsyncTailLogEntries) {
  EXPECT_CALL(*mock_, AsyncTailLogEntries)
      .WillOnce([this](google::cloud::CompletionQueue const&,
                       std::unique_ptr<grpc::ClientContext> context,
                       TailLogEntriesRequest const&) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.TailLogEntries"));
        using ErrorStream =
            ::google::cloud::internal::AsyncStreamingReadRpcError<
                TailLogEntriesResponse>;
        return absl::make_unique<ErrorStream>(
            Status(StatusCode::kAborted, "uh-oh"));
      });
  GoldenKitchenSinkMetadata stub(mock_);

  google::cloud::CompletionQueue cq;
  TailLogEntriesRequest request;
  auto stream = stub.AsyncTailLogEntries(
      cq, absl::make_unique<grpc::ClientContext>(), request);

  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST_F(MetadataDecoratorTest, AsyncWriteObject) {
  EXPECT_CALL(*mock_, AsyncWriteObject)
      .WillOnce([this](google::cloud::CompletionQueue const&,
                       std::unique_ptr<grpc::ClientContext> context) {
        EXPECT_STATUS_OK(IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.TailLogEntries"));
        using ErrorStream =
            ::google::cloud::internal::AsyncStreamingWriteRpcError<
                WriteObjectRequest, WriteObjectResponse>;
        return absl::make_unique<ErrorStream>(
            Status(StatusCode::kAborted, "uh-oh"));
      });
  GoldenKitchenSinkMetadata stub(mock_);

  google::cloud::CompletionQueue cq;
  auto stream =
      stub.AsyncWriteObject(cq, absl::make_unique<grpc::ClientContext>());

  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
