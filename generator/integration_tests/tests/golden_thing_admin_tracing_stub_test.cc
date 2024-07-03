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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_tracing_stub.h"
#include "generator/integration_tests/tests/mock_golden_thing_admin_stub.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_propagator.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::cloud::testing_util::ValidatePropagator;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Unused;

auto constexpr kErrorCode = "ABORTED";

future<StatusOr<google::longrunning::Operation>> LongrunningError(
    Unused, std::shared_ptr<grpc::ClientContext> const& context, Unused,
    Unused) {
  ValidatePropagator(*context);
  EXPECT_TRUE(ThereIsAnActiveSpan());
  EXPECT_TRUE(OTelContextCaptured());
  return make_ready_future(
      StatusOr<google::longrunning::Operation>(internal::AbortedError("fail")));
}

TEST(GoldenThingAdminTracingStubTest, ListDatabases) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabases)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  auto result = under_test.ListDatabases(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/ListDatabases"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncCreateDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCreateDatabase).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncCreateDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/CreateDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, GetDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabase)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  auto result = under_test.GetDatabase(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/GetDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncUpdateDatabaseDdl) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncUpdateDatabaseDdl).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncUpdateDatabaseDdl(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenThingAdmin/"
                    "UpdateDatabaseDdl"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, DropDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  auto result = under_test.DropDatabase(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/DropDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, GetDatabaseDdl) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetDatabaseDdl)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  auto result = under_test.GetDatabaseDdl(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/GetDatabaseDdl"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, SetIamPolicy) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  auto result = under_test.SetIamPolicy(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/SetIamPolicy"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, GetIamPolicy) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  auto result = under_test.GetIamPolicy(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/GetIamPolicy"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, TestIamPermissions) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  auto result = under_test.TestIamPermissions(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenThingAdmin/"
                    "TestIamPermissions"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncCreateBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCreateBackup).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::CreateBackupRequest request;
  CompletionQueue cq;
  auto result =
      under_test.AsyncCreateBackup(cq, std::make_shared<grpc::ClientContext>(),
                                   internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/CreateBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, GetBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, GetBackup)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  auto result = under_test.GetBackup(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenThingAdmin/GetBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, UpdateBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, UpdateBackup)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::UpdateBackupRequest request;
  auto result = under_test.UpdateBackup(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/UpdateBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, DeleteBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DeleteBackup)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::DeleteBackupRequest request;
  auto result = under_test.DeleteBackup(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/DeleteBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, ListBackups) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListBackups)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupsRequest request;
  auto result = under_test.ListBackups(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/ListBackups"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncRestoreDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncRestoreDatabase).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncRestoreDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/RestoreDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, ListDatabaseOperations) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListDatabaseOperations)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  auto result = under_test.ListDatabaseOperations(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenThingAdmin/"
                    "ListDatabaseOperations"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, ListBackupOperations) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, ListBackupOperations)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        return internal::AbortedError("fail");
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  auto result = under_test.ListBackupOperations(context, Options{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.test.admin.database.v1.GoldenThingAdmin/"
                    "ListBackupOperations"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncGetDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetDatabase)
      .WillOnce([](auto const&, auto context, auto, auto const&) {
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        return make_ready_future(
            StatusOr<google::test::admin::database::v1::Database>(
                internal::AbortedError("fail")));
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::GetDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncGetDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/GetDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncDropDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncDropDatabase)
      .WillOnce([](auto const&, auto context, auto, auto const&) {
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        return make_ready_future(internal::AbortedError("fail"));
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::test::admin::database::v1::DropDatabaseRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncDropDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "google.test.admin.database.v1.GoldenThingAdmin/DropDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncGetOperation) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).WillOnce(LongrunningError);

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::longrunning::GetOperationRequest request;
  CompletionQueue cq;
  auto result =
      under_test.AsyncGetOperation(cq, std::make_shared<grpc::ClientContext>(),
                                   internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.longrunning.Operations/GetOperation"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingStubTest, AsyncCancelOperation) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](auto const&, auto context, auto, auto const&) {
        ValidatePropagator(*context);
        EXPECT_TRUE(ThereIsAnActiveSpan());
        EXPECT_TRUE(OTelContextCaptured());
        return make_ready_future(internal::AbortedError("fail"));
      });

  auto under_test = GoldenThingAdminTracingStub(mock);
  google::longrunning::CancelOperationRequest request;
  CompletionQueue cq;
  auto result = under_test.AsyncCancelOperation(
      cq, std::make_shared<grpc::ClientContext>(),
      internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(result.get(), StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("google.longrunning.Operations/CancelOperation"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<std::string>("grpc.peer", _),
              OTelAttribute<std::string>("gl-cpp.status_code", kErrorCode)))));
}

TEST(MakeGoldenThingAdminTracingStub, OpenTelemetry) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce([](auto& context, auto const&, auto const&) {
        ValidatePropagator(context);
        return internal::AbortedError("fail");
      });

  auto under_test = MakeGoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  auto result = under_test->DropDatabase(context, Options{}, {});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Not(IsEmpty()));
}

#else

using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

TEST(MakeGoldenThingAdminTracingStub, NoOpenTelemetry) {
  auto mock = std::make_shared<MockGoldenThingAdminStub>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenThingAdminTracingStub(mock);
  grpc::ClientContext context;
  auto result = under_test->DropDatabase(context, Options{}, {});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
