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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_rest_metadata_decorator.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_rest_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::MockGoldenKitchenSinkRestStub;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::IsEmpty;

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

TEST(KitchenSinkRestMetadataDecoratorTest, FormatServerTimeoutMilliseconds) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce([](rest_internal::RestContext& context,
                   google::test::admin::database::v1::
                       GenerateAccessTokenRequest const&) {
        EXPECT_THAT(context.GetHeader("x-server-timeout"), Contains("3.141"));
        return TransientError();
      })
      .WillOnce([](rest_internal::RestContext& context,
                   google::test::admin::database::v1::
                       GenerateAccessTokenRequest const&) {
        EXPECT_THAT(context.GetHeader("x-server-timeout"),
                    Contains("3600.000"));
        return TransientError();
      })
      .WillOnce([](rest_internal::RestContext& context,
                   google::test::admin::database::v1::
                       GenerateAccessTokenRequest const&) {
        EXPECT_THAT(context.GetHeader("x-server-timeout"), Contains("0.123"));
        return TransientError();
      });

  GoldenKitchenSinkRestMetadata stub(mock);
  {
    internal::OptionsSpan span(
        Options{}.set<ServerTimeoutOption>(std::chrono::milliseconds(3141)));
    rest_internal::RestContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, request);
    EXPECT_EQ(TransientError(), status.status());
  }
  {
    internal::OptionsSpan span(
        Options{}.set<ServerTimeoutOption>(std::chrono::milliseconds(3600000)));
    rest_internal::RestContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, request);
    EXPECT_EQ(TransientError(), status.status());
  }
  {
    internal::OptionsSpan span(
        Options{}.set<ServerTimeoutOption>(std::chrono::milliseconds(123)));
    rest_internal::RestContext context;
    google::test::admin::database::v1::GenerateAccessTokenRequest request;
    auto status = stub.GenerateAccessToken(context, request);
    EXPECT_EQ(TransientError(), status.status());
  }
}

TEST(KitchenSinkRestMetadataDecoratorTest, GenerateAccessToken) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce([](rest_internal::RestContext& context,
                   google::test::admin::database::v1::
                       GenerateAccessTokenRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::ApiClientHeader("generator")));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  internal::OptionsSpan span(Options{});
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::GenerateAccessTokenRequest request;
  auto status = stub.GenerateAccessToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(KitchenSinkRestMetadataDecoratorTest, GenerateIdToken) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce(
          [](rest_internal::RestContext& context,
             google::test::admin::database::v1::GenerateIdTokenRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains(google::cloud::internal::ApiClientHeader(
                            "generator")));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"),
                        Contains("test-user-project"));
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  internal::OptionsSpan span(
      Options{}.set<UserProjectOption>("test-user-project"));
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::GenerateIdTokenRequest request;
  auto status = stub.GenerateIdToken(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(KitchenSinkRestMetadataDecoratorTest, WriteLogEntries) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .WillOnce(
          [](rest_internal::RestContext& context,
             google::test::admin::database::v1::WriteLogEntriesRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains(google::cloud::internal::ApiClientHeader(
                            "generator")));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"),
                        Contains("test-quota-user"));
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  internal::OptionsSpan span(Options{}.set<QuotaUserOption>("test-quota-user"));
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::WriteLogEntriesRequest request;
  auto status = stub.WriteLogEntries(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(KitchenSinkRestMetadataDecoratorTest, ListLogs) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce([](rest_internal::RestContext& context,
                   google::test::admin::database::v1::ListLogsRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::ApiClientHeader("generator")));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ListLogsRequest request;
  auto status = stub.ListLogs(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(KitchenSinkRestMetadataDecoratorTest, ListServiceAccountKeys) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce([](rest_internal::RestContext& context,
                   google::test::admin::database::v1::
                       ListServiceAccountKeysRequest const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::ApiClientHeader("generator")));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"),
                    Contains("test-user-project"));
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"),
                    Contains("test-quota-user"));
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  internal::OptionsSpan span(Options{}
                                 .set<QuotaUserOption>("test-quota-user")
                                 .set<UserProjectOption>("test-user-project"));
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  auto status = stub.ListServiceAccountKeys(context, request);
  EXPECT_EQ(TransientError(), status.status());
}

TEST(KitchenSinkRestMetadataDecoratorTest, DoNothing) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, DoNothing)
      .WillOnce([](rest_internal::RestContext& context,
                   google::protobuf::Empty const&) {
        EXPECT_THAT(
            context.GetHeader("x-goog-api-client"),
            Contains(google::cloud::internal::ApiClientHeader("generator")));
        EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
        EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
        return TransientError();
      });

  internal::OptionsSpan span(Options{});
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::protobuf::Empty request;
  auto status = stub.DoNothing(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(KitchenSinkRestMetadataDecoratorTest, ExplicitRouting) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting1)
      .WillOnce(
          [](rest_internal::RestContext& context,
             google::test::admin::database::v1::ExplicitRoutingRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains(google::cloud::internal::ApiClientHeader(
                            "generator")));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"),
                        AnyOf(Contains("table_location=instances/"
                                       "instance_bar&routing_id=prof_qux"),
                              Contains("routing_id=prof_qux&table_location="
                                       "instances/instance_bar")));
            return TransientError();
          });

  internal::OptionsSpan span(Options{});
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name(
      "projects/proj_foo/instances/instance_bar/tables/table_baz");
  request.set_app_profile_id("profiles/prof_qux");

  auto status = stub.ExplicitRouting1(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(KitchenSinkRestMetadataDecoratorTest,
     ExplicitRoutingDoesNotSendEmptyParams) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting1)
      .WillOnce(
          [](rest_internal::RestContext& context,
             google::test::admin::database::v1::ExplicitRoutingRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains(google::cloud::internal::ApiClientHeader(
                            "generator")));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
            return TransientError();
          });

  internal::OptionsSpan span(Options{});
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name("does-not-match");

  auto status = stub.ExplicitRouting1(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(KitchenSinkRestMetadataDecoratorTest, ExplicitRoutingNoRegexNeeded) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting2)
      .WillOnce(
          [](rest_internal::RestContext& context,
             google::test::admin::database::v1::ExplicitRoutingRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains(google::cloud::internal::ApiClientHeader(
                            "generator")));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"),
                        Contains("no_regex_needed=used"));
            return TransientError();
          });

  internal::OptionsSpan span(Options{});
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.set_table_name("used");
  request.set_no_regex_needed("ignored");
  auto status = stub.ExplicitRouting2(context, request);
  EXPECT_EQ(TransientError(), status);
}

TEST(KitchenSinkRestMetadataDecoratorTest, ExplicitRoutingNestedField) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting2)
      .WillOnce(
          [](rest_internal::RestContext& context,
             google::test::admin::database::v1::ExplicitRoutingRequest const&) {
            EXPECT_THAT(context.GetHeader("x-goog-api-client"),
                        Contains(google::cloud::internal::ApiClientHeader(
                            "generator")));
            EXPECT_THAT(context.GetHeader("x-goog-user-project"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-quota-user"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-server-timeout"), IsEmpty());
            EXPECT_THAT(context.GetHeader("x-goog-request-params"),
                        Contains("routing_id=value"));
            return TransientError();
          });

  internal::OptionsSpan span(Options{});
  GoldenKitchenSinkRestMetadata stub(mock);
  rest_internal::RestContext context;
  google::test::admin::database::v1::ExplicitRoutingRequest request;
  request.mutable_nested1()->mutable_nested2()->set_value("value");
  auto status = stub.ExplicitRouting2(context, request);
  EXPECT_EQ(TransientError(), status);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
