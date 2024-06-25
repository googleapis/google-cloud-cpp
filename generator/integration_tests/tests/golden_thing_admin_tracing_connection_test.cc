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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_tracing_connection.h"
#include "generator/integration_tests/golden/v1/mocks/mock_golden_thing_admin_connection.h"
#include "generator/integration_tests/test.pb.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_mocks::MockGoldenThingAdminConnection;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::OTelContextCaptured;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::google::test::admin::database::v1::Backup;
using ::google::test::admin::database::v1::Database;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

auto constexpr kErrorCode = static_cast<int>(StatusCode::kAborted);

TEST(GoldenThingAdminTracingConnectionTest, Options) {
  struct TestOption {
    using Type = int;
  };

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(Options{}.set<TestOption>(5)));

  auto under_test = GoldenThingAdminTracingConnection(mock);
  auto options = under_test.options();
  EXPECT_EQ(5, options.get<TestOption>());
}

TEST(GoldenThingAdminTracingConnectionTest, ListDatabases) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, ListDatabases).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return mocks::MakeStreamRange<Database>({}, internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::ListDatabasesRequest request;
  auto stream = under_test.ListDatabases(request);
  auto it = stream.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, stream.end());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::ListDatabases"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, CreateDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, CreateDatabase(_)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<Database>>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  auto result = under_test.CreateDatabase(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::CreateDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, CreateDatabaseStart) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, CreateDatabase(_, _, _)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return StatusOr<google::longrunning::Operation>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::CreateDatabaseRequest request;
  auto result =
      under_test.CreateDatabase(ExperimentalTag{}, NoAwaitTag{}, request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::CreateDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, CreateDatabaseAwait) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, CreateDatabase(_, _)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<Database>>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::longrunning::Operation operation;
  auto result = under_test.CreateDatabase(ExperimentalTag{}, operation).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::CreateDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, GetDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, GetDatabase).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::GetDatabaseRequest request;
  auto result = under_test.GetDatabase(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::GetDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, UpdateDatabaseDdl) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  using ::google::test::admin::database::v1::UpdateDatabaseDdlMetadata;
  EXPECT_CALL(*mock, UpdateDatabaseDdl(_)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<UpdateDatabaseDdlMetadata>>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  auto result = under_test.UpdateDatabaseDdl(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::UpdateDatabaseDdl"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, DropDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, DropDatabase).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::DropDatabaseRequest request;
  auto result = under_test.DropDatabase(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::DropDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, GetDatabaseDdl) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, GetDatabaseDdl).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  auto result = under_test.GetDatabaseDdl(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::GetDatabaseDdl"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, SetIamPolicy) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, SetIamPolicy).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::iam::v1::SetIamPolicyRequest request;
  auto result = under_test.SetIamPolicy(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::SetIamPolicy"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, GetIamPolicy) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::iam::v1::GetIamPolicyRequest request;
  auto result = under_test.GetIamPolicy(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::GetIamPolicy"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, TestIamPermissions) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, TestIamPermissions).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::iam::v1::TestIamPermissionsRequest request;
  auto result = under_test.TestIamPermissions(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "golden_v1::GoldenThingAdminConnection::TestIamPermissions"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, CreateBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, CreateBackup(_)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<Backup>>(internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::CreateBackupRequest request;
  auto result = under_test.CreateBackup(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::CreateBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, GetBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, GetBackup).WillOnce([]() {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::GetBackupRequest request;
  auto result = under_test.GetBackup(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::GetBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, UpdateBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, UpdateBackup).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::UpdateBackupRequest request;
  auto result = under_test.UpdateBackup(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::UpdateBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, DeleteBackup) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, DeleteBackup).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::DeleteBackupRequest request;
  auto result = under_test.DeleteBackup(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::DeleteBackup"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, ListBackups) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, ListBackups).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return mocks::MakeStreamRange<Backup>({}, internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::ListBackupsRequest request;
  auto stream = under_test.ListBackups(request);
  auto it = stream.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, stream.end());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::ListBackups"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, RestoreDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, RestoreDatabase(_)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<Database>>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  auto result = under_test.RestoreDatabase(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::RestoreDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, ListDatabaseOperations) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, ListDatabaseOperations).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return mocks::MakeStreamRange<google::longrunning::Operation>(
        {}, internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  auto stream = under_test.ListDatabaseOperations(request);
  auto it = stream.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, stream.end());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "golden_v1::GoldenThingAdminConnection::ListDatabaseOperations"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, ListBackupOperations) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, ListBackupOperations).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return mocks::MakeStreamRange<google::longrunning::Operation>(
        {}, internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  auto stream = under_test.ListBackupOperations(request);
  auto it = stream.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, stream.end());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed(
              "golden_v1::GoldenThingAdminConnection::ListBackupOperations"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, LongRunningWithoutRouting) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, LongRunningWithoutRouting(_)).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<Database>>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  auto result = under_test.LongRunningWithoutRouting(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::"
                    "LongRunningWithoutRouting"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, AsyncGetDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, AsyncGetDatabase).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future<StatusOr<Database>>(
        internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::GetDatabaseRequest request;
  auto result = under_test.AsyncGetDatabase(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::AsyncGetDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(GoldenThingAdminTracingConnectionTest, AsyncDropDatabase) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, AsyncDropDatabase).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    EXPECT_TRUE(OTelContextCaptured());
    return make_ready_future(internal::AbortedError("fail"));
  });

  auto under_test = GoldenThingAdminTracingConnection(mock);
  google::test::admin::database::v1::DropDatabaseRequest request;
  auto result = under_test.AsyncDropDatabase(request).get();
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("golden_v1::GoldenThingAdminConnection::AsyncDropDatabase"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", kErrorCode)))));
}

TEST(MakeGoldenThingAdminTracingConnection, TracingEnabled) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(EnableTracing(Options{})));
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenThingAdminTracingConnection(mock);
  auto result = under_test->DropDatabase({});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Not(IsEmpty()));
}

TEST(MakeGoldenThingAdminTracingConnection, TracingDisabled) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(DisableTracing(Options{})));
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenThingAdminTracingConnection(mock);
  auto result = under_test->DropDatabase({});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

#else

TEST(MakeGoldenThingAdminTracingConnection, NoOpenTelemetry) {
  auto mock = std::make_shared<MockGoldenThingAdminConnection>();
  EXPECT_CALL(*mock, DropDatabase)
      .WillOnce(Return(internal::AbortedError("fail")));

  auto under_test = MakeGoldenThingAdminTracingConnection(mock);
  auto result = under_test->DropDatabase({});
  EXPECT_THAT(result, StatusIs(StatusCode::kAborted));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
