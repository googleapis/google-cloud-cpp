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
#include "absl/strings/str_split.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_internal::MockGoldenKitchenSinkStub;
using ::google::cloud::golden_internal::MockStreamingReadRpc;
using ::google::cloud::golden_internal::MockStreamingWriteRpc;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::_;
using ::testing::AnyOf;
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

  void IsContextMDValid(grpc::ClientContext& context, std::string const& method,
                        google::protobuf::Message const& request) {
    return validate_metadata_fixture_.IsContextMDValid(
        context, method, request,
        google::cloud::internal::ApiClientHeader("generator"));
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
                           GenerateAccessTokenRequest const& request) {
        IsContextMDValid(context,
                         "google.test.admin.database.v1.GoldenKitchenSink."
                         "GenerateAccessToken",
                         request);
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
                           GenerateIdTokenRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.GenerateIdToken",
            request);
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
                           WriteLogEntriesRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.WriteLogEntries",
            request);
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
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::ListLogsRequest const&
                           request) {
        IsContextMDValid(
            context, "google.test.admin.database.v1.GoldenKitchenSink.ListLogs",
            request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my_project");
  auto status = stub.ListLogs(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListServiceAccountKeys) {
  EXPECT_CALL(*mock_, ListServiceAccountKeys)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ListServiceAccountKeysRequest const& request) {
        IsContextMDValid(context,
                         "google.test.admin.database.v1.GoldenKitchenSink."
                         "ListServiceAccountKeys",
                         request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  request.set_name("projects/my-project/serviceAccounts/foo@bar.com");
  auto status = stub.ListServiceAccountKeys(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, StreamingRead) {
  EXPECT_CALL(*mock_, StreamingRead)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context,
                       Request const& request) {
        auto mock_response = absl::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*mock_response, Read)
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingRead",
            request);
        return mock_response;
      });
  GoldenKitchenSinkMetadata stub(mock_);
  auto response =
      stub.StreamingRead(absl::make_unique<grpc::ClientContext>(), Request{});
  EXPECT_THAT(absl::get<Status>(response->Read()), Not(IsOk()));
}

TEST_F(MetadataDecoratorTest, StreamingWrite) {
  EXPECT_CALL(*mock_, StreamingWrite)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context) {
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingWrite",
            Request{});

        auto stream = absl::make_unique<MockStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(true))
            .WillOnce(Return(false));
        auto response = Response{};
        response.set_response("test-only");
        EXPECT_CALL(*stream, Close).WillOnce(Return(make_status_or(response)));
        return stream;
      });

  GoldenKitchenSinkMetadata stub(mock_);
  auto stream = stub.StreamingWrite(absl::make_unique<grpc::ClientContext>());
  EXPECT_TRUE(stream->Write(Request{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(Request{}, grpc::WriteOptions()));
  auto response = stream->Close();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->response(), "test-only");
}

TEST_F(MetadataDecoratorTest, AsyncStreamingRead) {
  EXPECT_CALL(*mock_, AsyncStreamingRead)
      .WillOnce([this](google::cloud::CompletionQueue const&,
                       std::unique_ptr<grpc::ClientContext> context,
                       Request const& request) {
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingRead",
            request);
        using ErrorStream =
            ::google::cloud::internal::AsyncStreamingReadRpcError<Response>;
        return absl::make_unique<ErrorStream>(
            Status(StatusCode::kAborted, "uh-oh"));
      });
  GoldenKitchenSinkMetadata stub(mock_);

  google::cloud::CompletionQueue cq;
  auto stream = stub.AsyncStreamingRead(
      cq, absl::make_unique<grpc::ClientContext>(), Request{});

  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST_F(MetadataDecoratorTest, AsyncStreamingWrite) {
  EXPECT_CALL(*mock_, AsyncStreamingWrite)
      .WillOnce([this](google::cloud::CompletionQueue const&,
                       std::unique_ptr<grpc::ClientContext> context) {
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingWrite",
            Request{});
        using ErrorStream =
            ::google::cloud::internal::AsyncStreamingWriteRpcError<Request,
                                                                   Response>;
        return absl::make_unique<ErrorStream>(
            Status(StatusCode::kAborted, "uh-oh"));
      });
  GoldenKitchenSinkMetadata stub(mock_);

  google::cloud::CompletionQueue cq;
  auto stream =
      stub.AsyncStreamingWrite(cq, absl::make_unique<grpc::ClientContext>());

  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST_F(MetadataDecoratorTest, ExplicitRouting) {
  // In `test.proto` we define the `ExplicitRouting1` rpc to have the same
  // routing parameters as Example 9 from the `google.api.routing` proto.
  //
  // In this test, we will use the request message provided in the
  // `google.api.routing` examples:
  //
  // https://github.com/googleapis/googleapis/blob/f46dc249e1987a6bef1a70a371e8288ea4c17481/google/api/routing.proto#L57-L60
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name(
      "projects/proj_foo/instances/instance_bar/tables/table_baz");
  request.set_app_profile_id("profiles/prof_qux");

  // We verify the routing metadata against the expectations provided in
  // `google.api.routing` for Example 9:
  //
  // https://github.com/googleapis/googleapis/blob/f46dc249e1987a6bef1a70a371e8288ea4c17481/google/api/routing.proto#L387-L390
  std::string expected1 = "table_location=instances/instance_bar";
  std::string expected2 = "routing_id=prof_qux";

  EXPECT_CALL(*mock_, ExplicitRouting1)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting1",
            request);
        return Status();
      })
      .WillOnce([&, this](grpc::ClientContext& context,
                          google::test::admin::database::v1::
                              ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-request-params",
                                  // We use `AnyOf` because it does not matter
                                  // which order the parameters are added in.
                                  AnyOf(expected1 + "&" + expected2,
                                        expected2 + "&" + expected1))));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context1;
  grpc::ClientContext context2;
  // We make the same call twice. In the first call, we use `IsContextMDValid`
  // to verify expectations. In the second call, we verify the routing
  // parameters by hand. This gives us extra confidence in `IsContextMDValid`
  // which is reasonably complex, but untested. (We cannot do them both in the
  // same call, because the `grpc::ClientContext` is consumed in order to
  // extract its metadata).
  (void)stub.ExplicitRouting1(context1, request);
  (void)stub.ExplicitRouting1(context2, request);
}

TEST_F(MetadataDecoratorTest, ExplicitRoutingDoesNotSendEmptyParams) {
  EXPECT_CALL(*mock_, ExplicitRouting1)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting1",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Not(Contains(Pair("x-goog-request-params", _))));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context1;
  grpc::ClientContext context2;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name("does-not-match");
  // We make the same call twice. In the first call, we use `IsContextMDValid`
  // to verify expectations. In the second call, we verify the routing
  // parameters by hand. This gives us extra confidence in `IsContextMDValid`
  // which is reasonably complex, but untested. (We cannot do them both in the
  // same call, because the `grpc::ClientContext` is consumed in order to
  // extract its metadata).
  (void)stub.ExplicitRouting1(context1, request);
  (void)stub.ExplicitRouting1(context2, request);
}

TEST_F(MetadataDecoratorTest, ExplicitRoutingNoRegexNeeded) {
  EXPECT_CALL(*mock_, ExplicitRouting2)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting2",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Contains(Pair("x-goog-request-params",
                                           "no_regex_needed=used")));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context1;
  grpc::ClientContext context2;
  // Note that the `app_profile_id` field is not set.
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name("used");
  request.set_no_regex_needed("ignored");
  // We make the same call twice. In the first call, we use `IsContextMDValid`
  // to verify expectations. In the second call, we verify the routing
  // parameters by hand. This gives us extra confidence in `IsContextMDValid`
  // which is reasonably complex, but untested. (We cannot do them both in the
  // same call, because the `grpc::ClientContext` is consumed in order to
  // extract its metadata).
  (void)stub.ExplicitRouting2(context1, request);
  (void)stub.ExplicitRouting2(context2, request);
}

TEST_F(MetadataDecoratorTest, ExplicitRoutingNestedField) {
  EXPECT_CALL(*mock_, ExplicitRouting2)
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting2",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Contains(Pair("x-goog-request-params",
                                           "routing_id=value")));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_);
  grpc::ClientContext context1;
  grpc::ClientContext context2;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.mutable_nested1()->mutable_nested2()->set_value("value");
  // We make the same call twice. In the first call, we use `IsContextMDValid`
  // to verify expectations. In the second call, we verify the routing
  // parameters by hand. This gives us extra confidence in `IsContextMDValid`
  // which is reasonably complex, but untested. (We cannot do them both in the
  // same call, because the `grpc::ClientContext` is consumed in order to
  // extract its metadata).
  (void)stub.ExplicitRouting2(context1, request);
  (void)stub.ExplicitRouting2(context2, request);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
