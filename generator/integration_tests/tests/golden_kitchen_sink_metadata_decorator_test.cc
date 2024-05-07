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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_metadata_decorator.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::MockGoldenKitchenSinkStub;
using ::google::cloud::golden_v1_internal::MockStreamingReadRpc;
using ::google::cloud::golden_v1_internal::MockStreamingWriteRpc;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

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
    validate_metadata_fixture_.IsContextMDValid(
        context, method, request,
        google::cloud::internal::GeneratedLibClientHeader());
  }

  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

  std::shared_ptr<MockGoldenKitchenSinkStub> mock_;

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

TEST_F(MetadataDecoratorTest, ExplicitApiClientHeader) {
  // We use knowledge of the implementation to assert that testing a single RPC
  // is sufficient.
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-api-client", "test-client-header")));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {}, "test-client-header");
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto status = stub.GenerateAccessToken(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

/// Verify the x-goog-user-project metadata is set.
TEST_F(MetadataDecoratorTest, UserProject) {
  // We use knowledge of the implementation to assert that testing a single RPC
  // is sufficient.
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, Not(Contains(Pair("x-goog-user-project", _))));
        return TransientError();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    Contains(Pair("x-goog-user-project", "test-user-project")));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  // First try without any UserProjectOption
  {
    grpc::ClientContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, Options{}, request);
    EXPECT_EQ(TransientError(), status.status());
  }
  // Then try with a UserProjectOption
  {
    grpc::ClientContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(
        context, Options{}.set<UserProjectOption>("test-user-project"),
        request);
    EXPECT_EQ(TransientError(), status.status());
  }
}

TEST_F(MetadataDecoratorTest, CustomHeaders) {
  // We use knowledge of the implementation to assert that testing a single RPC
  // is sufficient.
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, ::testing::UnorderedElementsAre(
                                  Pair("x-goog-api-version", _),
                                  Pair("x-goog-request-params", _),
                                  Pair("x-goog-api-client", _)));
        return TransientError();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(Pair("x-goog-api-version", _),
                                         Pair("x-goog-request-params", _),
                                         Pair("x-goog-api-client", _),
                                         Pair("header-key0", "header-value0"),
                                         Pair("header-key1", "header-value1"),
                                         Pair("header-key1", "header-value2")));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  // First try without any CustomHeadersOption
  {
    grpc::ClientContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, Options{}, request);
    EXPECT_EQ(TransientError(), status.status());
  }
  // Then try with a CustomHeadersOption
  {
    grpc::ClientContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(
        context,
        Options{}.set<CustomHeadersOption>({{"header-key0", "header-value0"},
                                            {"header-key1", "header-value1"},
                                            {"header-key1", "header-value2"}}),
        request);
    EXPECT_EQ(TransientError(), status.status());
  }
}

TEST_F(MetadataDecoratorTest, GenerateAccessToken) {
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const& request) {
        IsContextMDValid(context,
                         "google.test.admin.database.v1.GoldenKitchenSink."
                         "GenerateAccessToken",
                         request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateAccessToken(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, GenerateIdToken) {
  EXPECT_CALL(*mock_, GenerateIdToken)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateIdTokenRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.GenerateIdToken",
            request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  request.set_name("projects/-/serviceAccounts/foo@bar.com");
  auto status = stub.GenerateIdToken(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, WriteLogEntries) {
  EXPECT_CALL(*mock_, WriteLogEntries)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           WriteLogEntriesRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.WriteLogEntries",
            request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto status = stub.WriteLogEntries(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListLogs) {
  EXPECT_CALL(*mock_, ListLogs)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::ListLogsRequest const&
                           request) {
        IsContextMDValid(
            context, "google.test.admin.database.v1.GoldenKitchenSink.ListLogs",
            request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my_project");
  auto status = stub.ListLogs(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, ListServiceAccountKeys) {
  EXPECT_CALL(*mock_, ListServiceAccountKeys)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ListServiceAccountKeysRequest const& request) {
        IsContextMDValid(context,
                         "google.test.admin.database.v1.GoldenKitchenSink."
                         "ListServiceAccountKeys",
                         request);
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  request.set_name("projects/my-project/serviceAccounts/foo@bar.com");
  auto status = stub.ListServiceAccountKeys(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST_F(MetadataDecoratorTest, StreamingRead) {
  EXPECT_CALL(*mock_, StreamingRead)
      .WillOnce([this](auto context, Options const&, Request const& request) {
        auto mock_response = std::make_unique<MockStreamingReadRpc>();
        EXPECT_CALL(*mock_response, Read)
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingRead",
            request);
        return mock_response;
      });
  GoldenKitchenSinkMetadata stub(mock_, {});
  auto response = stub.StreamingRead(std::make_shared<grpc::ClientContext>(),
                                     Options{}, Request{});
  EXPECT_THAT(response->Read(), VariantWith<Status>(Not(IsOk())));
}

TEST_F(MetadataDecoratorTest, StreamingWrite) {
  EXPECT_CALL(*mock_, StreamingWrite)
      .WillOnce([this](auto context, Options const&) {
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingWrite",
            Request{});

        auto stream = std::make_unique<MockStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(true))
            .WillOnce(Return(false));
        auto response = Response{};
        response.set_response("test-only");
        EXPECT_CALL(*stream, Close).WillOnce(Return(make_status_or(response)));
        return stream;
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  auto stream =
      stub.StreamingWrite(std::make_shared<grpc::ClientContext>(), Options{});
  EXPECT_TRUE(stream->Write(Request{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(Request{}, grpc::WriteOptions()));
  auto response = stream->Close();
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->response(), "test-only");
}

TEST_F(MetadataDecoratorTest, AsyncStreamingRead) {
  EXPECT_CALL(*mock_, AsyncStreamingRead)
      .WillOnce([this](google::cloud::CompletionQueue const&, auto context,
                       auto, Request const& request) {
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingRead",
            request);
        using ErrorStream =
            ::google::cloud::internal::AsyncStreamingReadRpcError<Response>;
        return std::make_unique<ErrorStream>(
            Status(StatusCode::kAborted, "uh-oh"));
      });
  GoldenKitchenSinkMetadata stub(mock_, {});

  google::cloud::CompletionQueue cq;
  auto stream = stub.AsyncStreamingRead(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), Request{});

  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST_F(MetadataDecoratorTest, AsyncStreamingWrite) {
  EXPECT_CALL(*mock_, AsyncStreamingWrite)
      .WillOnce([this](google::cloud::CompletionQueue const&, auto context,
                       auto) {
        IsContextMDValid(
            *context,
            "google.test.admin.database.v1.GoldenKitchenSink.StreamingWrite",
            Request{});
        using ErrorStream =
            ::google::cloud::internal::AsyncStreamingWriteRpcError<Request,
                                                                   Response>;
        return std::make_unique<ErrorStream>(
            Status(StatusCode::kAborted, "uh-oh"));
      });
  GoldenKitchenSinkMetadata stub(mock_, {});

  google::cloud::CompletionQueue cq;
  auto stream = stub.AsyncStreamingWrite(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}));

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
  // https://github.com/googleapis/googleapis/blob/70147caca58ebf4c8cd7b96f5d569a72723e11c1/google/api/routing.proto#L57-L60
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name(
      "projects/proj_foo/instances/instance_bar/tables/table_baz");
  request.set_app_profile_id("profiles/prof_qux");

  // We verify the routing metadata against the expectations provided in
  // `google.api.routing` for Example 9:
  //
  // https://github.com/googleapis/googleapis/blob/70147caca58ebf4c8cd7b96f5d569a72723e11c1/google/api/routing.proto#L387-L390
  std::string expected1 = "table_location=instances%2Finstance_bar";
  std::string expected2 = "routing_id=prof_qux";

  EXPECT_CALL(*mock_, ExplicitRouting1)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting1",
            request);
        return Status();
      })
      .WillOnce([&, this](grpc::ClientContext& context, Options const&,
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

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context1;
  grpc::ClientContext context2;
  // We make the same call twice. In the first call, we use `IsContextMDValid`
  // to verify expectations. In the second call, we verify the routing
  // parameters by hand. This gives us extra confidence in `IsContextMDValid`
  // which is reasonably complex, but untested. (We cannot do them both in the
  // same call, because the `grpc::ClientContext` is consumed in order to
  // extract its metadata).
  (void)stub.ExplicitRouting1(context1, Options{}, request);
  (void)stub.ExplicitRouting1(context2, Options{}, request);
}

TEST_F(MetadataDecoratorTest, ExplicitRoutingDoesNotSendEmptyParams) {
  EXPECT_CALL(*mock_, ExplicitRouting1)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting1",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Not(Contains(Pair("x-goog-request-params", _))));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
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
  (void)stub.ExplicitRouting1(context1, Options{}, request);
  (void)stub.ExplicitRouting1(context2, Options{}, request);
}

TEST_F(MetadataDecoratorTest, ExplicitRoutingNoRegexNeeded) {
  EXPECT_CALL(*mock_, ExplicitRouting2)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting2",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Contains(Pair("x-goog-request-params",
                                           "no_regex_needed=used")));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
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
  (void)stub.ExplicitRouting2(context1, Options{}, request);
  (void)stub.ExplicitRouting2(context2, Options{}, request);
}

TEST_F(MetadataDecoratorTest, ExplicitRoutingNestedField) {
  EXPECT_CALL(*mock_, ExplicitRouting2)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting2",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Contains(Pair("x-goog-request-params",
                                           "routing_id=value")));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
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
  (void)stub.ExplicitRouting2(context1, Options{}, request);
  (void)stub.ExplicitRouting2(context2, Options{}, request);
}

TEST_F(MetadataDecoratorTest, UrlEncodeRoutingParam) {
  EXPECT_CALL(*mock_, ExplicitRouting2)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const& request) {
        IsContextMDValid(
            context,
            "google.test.admin.database.v1.GoldenKitchenSink.ExplicitRouting2",
            request);
        return Status();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           ExplicitRoutingRequest const&) {
        auto headers = GetMetadata(context);
        EXPECT_THAT(headers, Contains(Pair("x-goog-request-params",
                                           "routing_id=%2Fvalue")));
        return Status();
      });

  GoldenKitchenSinkMetadata stub(mock_, {});
  grpc::ClientContext context1;
  grpc::ClientContext context2;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.mutable_nested1()->mutable_nested2()->set_value("/value");
  // We make the same call twice. In the first call, we use `IsContextMDValid`
  // to verify expectations. In the second call, we verify the routing
  // parameters by hand. This gives us extra confidence in `IsContextMDValid`
  // which is reasonably complex, but untested. (We cannot do them both in the
  // same call, because the `grpc::ClientContext` is consumed in order to
  // extract its metadata).
  (void)stub.ExplicitRouting2(context1, Options{}, request);
  (void)stub.ExplicitRouting2(context2, Options{}, request);
}

TEST_F(MetadataDecoratorTest, FixedMetadata) {
  // We use knowledge of the implementation to assert that testing a single RPC
  // is sufficient.
  EXPECT_CALL(*mock_, GenerateAccessToken)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::test::admin::database::v1::
                           GenerateAccessTokenRequest const&) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    AllOf(Contains(Pair("test-key-1", "test-value-1")),
                          Contains(Pair("test-key-2", "test-value-2"))));
        return TransientError();
      });

  GoldenKitchenSinkMetadata stub(
      mock_, {{"test-key-1", "test-value-1"}, {"test-key-2", "test-value-2"}});
  grpc::ClientContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto status = stub.GenerateAccessToken(context, Options{}, request);
  EXPECT_EQ(TransientError(), status.status());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
