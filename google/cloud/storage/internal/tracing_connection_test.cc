// Copyright 2023 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/storage/internal/tracing_connection.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::MockClient;
using ::google::cloud::storage::testing::MockObjectReadSource;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanIsRoot;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::Return;

TEST(TracingClientTest, Options) {
  struct TestOption {
    using Type = int;
  };

  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, options).WillOnce(Return(Options{}.set<TestOption>(42)));
  auto under_test = TracingConnection(mock);
  auto const options = under_test.options();
  EXPECT_EQ(42, options.get<TestOption>());
}

TEST(TracingClientTest, ListBuckets) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBuckets).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.ListBuckets(storage::internal::ListBucketsRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("storage::Client::ListBuckets"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateBucket) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateBucket).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.CreateBucket(storage::internal::CreateBucketRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("storage::Client::CreateBucket"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetBucketMetadata) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetBucketMetadata).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.GetBucketMetadata(
      storage::internal::GetBucketMetadataRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("storage::Client::GetBucketMetadata"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteBucket) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteBucket).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.DeleteBucket(storage::internal::DeleteBucketRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteBucket"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UpdateBucket) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UpdateBucket).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.UpdateBucket(storage::internal::UpdateBucketRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::UpdateBucket"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, PatchBucket) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, PatchBucket).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.PatchBucket(storage::internal::PatchBucketRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::PatchBucket"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetNativeBucketIamPolicy) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetNativeBucketIamPolicy).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.GetNativeBucketIamPolicy(
      storage::internal::GetBucketIamPolicyRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetNativeBucketIamPolicy"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, SetNativeBucketIamPolicy) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, SetNativeBucketIamPolicy).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.SetNativeBucketIamPolicy(
      storage::internal::SetNativeBucketIamPolicyRequest(
          "test-bucket",
          storage::NativeIamPolicy(/*bindings=*/{}, /*etag*/ "test-etag")));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::SetNativeBucketIamPolicy"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, TestBucketIamPermissions) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, TestBucketIamPermissions).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.TestBucketIamPermissions(
      storage::internal::TestBucketIamPermissionsRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::TestBucketIamPermissions"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, LockBucketRetentionPolicy) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, LockBucketRetentionPolicy).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.LockBucketRetentionPolicy(
      storage::internal::LockBucketRetentionPolicyRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::LockBucketRetentionPolicy"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, InsertObjectMedia) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, InsertObjectMedia).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.InsertObjectMedia(
      storage::internal::InsertObjectMediaRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::InsertObjectMedia"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CopyObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CopyObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.CopyObject(storage::internal::CopyObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::CopyObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetObjectMetadata) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetObjectMetadata).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.GetObjectMetadata(
      storage::internal::GetObjectMetadataRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetObjectMetadata"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ReadObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ReadObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.ReadObject(storage::internal::ReadObjectRangeRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ReadObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ReadObjectPartialSuccess) {
  using ::google::cloud::storage::internal::ObjectReadSource;
  using ::google::cloud::storage::internal::ReadSourceResult;

  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ReadObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    internal::EndSpan(*internal::MakeSpan("Read1"));
    internal::EndSpan(*internal::MakeSpan("Read2"));
    auto source = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*source, Read)
        .WillOnce(Return(ReadSourceResult{}))
        .WillOnce(Return(PermanentError()));
    return std::unique_ptr<ObjectReadSource>(std::move(source));
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.ReadObject(storage::internal::ReadObjectRangeRequest());
  ASSERT_STATUS_OK(actual);
  auto reader = *std::move(actual);

  auto const code = PermanentError().code();
  auto const msg = PermanentError().message();
  EXPECT_STATUS_OK(reader->Read(nullptr, 1024));
  EXPECT_THAT(reader->Read(nullptr, 1024), StatusIs(code));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      UnorderedElementsAre(
          AllOf(SpanNamed("storage::Client::ReadObject"),
                SpanHasInstrumentationScope(), SpanKindIsClient(), SpanIsRoot(),
                SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                SpanHasAttributes(OTelAttribute<std::string>(
                    "gl-cpp.status_code", StatusCodeToString(code)))),
          AllOf(SpanNamed("Read1"), Not(SpanIsRoot())),
          AllOf(SpanNamed("Read2"), Not(SpanIsRoot()))));
}

TEST(TracingClientTest, ListObjects) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjects).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.ListObjects(storage::internal::ListObjectsRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ListObjects"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.DeleteObject(storage::internal::DeleteObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UpdateObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UpdateObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.UpdateObject(storage::internal::UpdateObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::UpdateObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, MoveObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, MoveObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.MoveObject(storage::internal::MoveObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::MoveObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, PatchObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, PatchObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.PatchObject(storage::internal::PatchObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::PatchObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ComposeObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ComposeObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.ComposeObject(storage::internal::ComposeObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ComposeObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, RewriteObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, RewriteObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.RewriteObject(storage::internal::RewriteObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::RewriteObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, RestoreObject) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, RestoreObject).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.RestoreObject(storage::internal::RestoreObjectRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::RestoreObject"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateResumableUpload) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateResumableUpload).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.CreateResumableUpload(
      storage::internal::ResumableUploadRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(
          AllOf(SpanNamed("storage::Client::WriteObject/CreateResumableUpload"),
                SpanHasInstrumentationScope(), SpanKindIsClient(),
                SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                SpanHasAttributes(OTelAttribute<std::string>(
                    "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, QueryResumableUpload) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, QueryResumableUpload).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.QueryResumableUpload(
      storage::internal::QueryResumableUploadRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(
          AllOf(SpanNamed("storage::Client::WriteObject/QueryResumableUpload"),
                SpanHasInstrumentationScope(), SpanKindIsClient(),
                SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                SpanHasAttributes(OTelAttribute<std::string>(
                    "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteResumableUpload) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteResumableUpload).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.DeleteResumableUpload(
      storage::internal::DeleteResumableUploadRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteResumableUpload"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UploadChunk) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UploadChunk).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.UploadChunk(storage::internal::UploadChunkRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::WriteObject/UploadChunk"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ListBucketAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListBucketAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.ListBucketAcl(storage::internal::ListBucketAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ListBucketAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateBucketAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateBucketAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.CreateBucketAcl(storage::internal::CreateBucketAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::CreateBucketAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteBucketAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteBucketAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.DeleteBucketAcl(storage::internal::DeleteBucketAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteBucketAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetBucketAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetBucketAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.GetBucketAcl(storage::internal::GetBucketAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetBucketAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UpdateBucketAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UpdateBucketAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.UpdateBucketAcl(storage::internal::UpdateBucketAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::UpdateBucketAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, PatchBucketAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, PatchBucketAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.PatchBucketAcl(storage::internal::PatchBucketAclRequest(
          "test-bucket-name", "test-entity",
          storage::BucketAccessControlPatchBuilder()));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::PatchBucketAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ListObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.ListObjectAcl(storage::internal::ListObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ListObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.CreateObjectAcl(storage::internal::CreateObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::CreateObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.DeleteObjectAcl(storage::internal::DeleteObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.GetObjectAcl(storage::internal::GetObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UpdateObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UpdateObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.UpdateObjectAcl(storage::internal::UpdateObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::UpdateObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, PatchObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, PatchObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.PatchObjectAcl(storage::internal::PatchObjectAclRequest(
          "test-bucket-name", "test-object-name", "test-entity",
          storage::ObjectAccessControlPatchBuilder()));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::PatchObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ListDefaultObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListDefaultObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.ListDefaultObjectAcl(
      storage::internal::ListDefaultObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ListDefaultObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateDefaultObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateDefaultObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.CreateDefaultObjectAcl(
      storage::internal::CreateDefaultObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::CreateDefaultObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteDefaultObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteDefaultObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.DeleteDefaultObjectAcl(
      storage::internal::DeleteDefaultObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteDefaultObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetDefaultObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetDefaultObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.GetDefaultObjectAcl(
      storage::internal::GetDefaultObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetDefaultObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UpdateDefaultObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UpdateDefaultObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.UpdateDefaultObjectAcl(
      storage::internal::UpdateDefaultObjectAclRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::UpdateDefaultObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, PatchDefaultObjectAcl) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, PatchDefaultObjectAcl).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.PatchDefaultObjectAcl(
      storage::internal::PatchDefaultObjectAclRequest(
          "test-bucket-name", "test-entity",
          storage::ObjectAccessControlPatchBuilder()));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::PatchDefaultObjectAcl"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetServiceAccount) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetServiceAccount).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.GetServiceAccount(
      storage::internal::GetProjectServiceAccountRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetServiceAccount"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ListHmacKeys) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListHmacKeys).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.ListHmacKeys(
      storage::internal::ListHmacKeysRequest("test-project-id"));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ListHmacKeys"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateHmacKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateHmacKey).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.CreateHmacKey(storage::internal::CreateHmacKeyRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::CreateHmacKey"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteHmacKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteHmacKey).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.DeleteHmacKey(storage::internal::DeleteHmacKeyRequest(
          "test-project-id", "test-access-id"));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteHmacKey"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetHmacKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetHmacKey).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.GetHmacKey(storage::internal::GetHmacKeyRequest(
      "test-project-id", "test-access-id"));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetHmacKey"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, UpdateHmacKey) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, UpdateHmacKey).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.UpdateHmacKey(storage::internal::UpdateHmacKeyRequest(
          "test-project-id", "test-access-id", storage::HmacKeyMetadata()));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::UpdateHmacKey"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, SignBlob) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, SignBlob).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.SignBlob(storage::internal::SignBlobRequest(
      "test-service-account", "test-encoded-blob", {}));
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::SignBlob"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, ListNotifications) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, ListNotifications).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.ListNotifications(
      storage::internal::ListNotificationsRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::ListNotifications"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, CreateNotification) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, CreateNotification).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.CreateNotification(
      storage::internal::CreateNotificationRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::CreateNotification"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, GetNotification) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, GetNotification).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual =
      under_test.GetNotification(storage::internal::GetNotificationRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::GetNotification"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

TEST(TracingClientTest, DeleteNotification) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_shared<MockClient>();
  EXPECT_CALL(*mock, DeleteNotification).WillOnce([](auto const&) {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return PermanentError();
  });
  auto under_test = TracingConnection(mock);
  auto actual = under_test.DeleteNotification(
      storage::internal::DeleteNotificationRequest());
  auto const code = PermanentError().code();
  auto const code_str = StatusCodeToString(code);
  auto const msg = PermanentError().message();
  EXPECT_THAT(actual, StatusIs(code));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("storage::Client::DeleteNotification"),
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError, msg),
                  SpanHasAttributes(OTelAttribute<std::string>(
                      "gl-cpp.status_code", code_str)))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
