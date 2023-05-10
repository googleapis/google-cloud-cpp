// Copyright 2023 Google LLC
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

#include "google/cloud/bigtable/internal/data_tracing_connection.h"
#include "google/cloud/bigtable/mocks/mock_data_connection.h"
#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::bigtable_mocks::MockDataConnection;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SizeIs;
using ms = std::chrono::milliseconds;

auto constexpr kErrorCode = static_cast<int>(StatusCode::kAborted);
auto constexpr kTableName = "test-table";

bigtable::SingleRowMutation Mutation() {
  return bigtable::SingleRowMutation(
      "row", {bigtable::SetCell("fam", "col", ms(0), "val")});
}

TEST(DataTracingConnection, Options) {
  struct TestOption {
    using Type = int;
  };

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, options).WillOnce(Return(Options{}.set<TestOption>(5)));

  auto under_test = MakeDataTracingConnection(mock);
  auto options = under_test->options();
  EXPECT_EQ(5, options.get<TestOption>());
}

TEST(DataTracingConnection, Apply) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, Apply).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto status = under_test->Apply(kTableName, Mutation());
  EXPECT_THAT(status, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::Apply"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, AsyncApply) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncApply).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future(internal::AbortedError("fail"));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto status = under_test->AsyncApply(kTableName, Mutation());
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncApply"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, BulkApplySuccess) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, BulkApply).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return std::vector<bigtable::FailedMutation>{};
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto failures = under_test->BulkApply(kTableName, Mutation());
  EXPECT_THAT(failures, IsEmpty());

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::BulkApply"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.failed_mutations", 0),
                            SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.successful_mutations", 1)))));
}

TEST(DataTracingConnection, BulkApplyFailure) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, BulkApply).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return std::vector<bigtable::FailedMutation>{
        {internal::AbortedError("fail"), 1},
        {internal::AbortedError("fail"), 2}};
  });

  bigtable::BulkMutation mut;
  for (auto i = 0; i != 10; ++i) mut.push_back(Mutation());
  auto under_test = MakeDataTracingConnection(mock);
  auto failures = under_test->BulkApply(kTableName, std::move(mut));
  EXPECT_THAT(failures, SizeIs(2));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::BulkApply"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasAttributes(SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.failed_mutations", 2),
                            SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.successful_mutations", 8)))));
}

TEST(DataTracingConnection, AsyncBulkApplySuccess) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncBulkApply).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future(std::vector<bigtable::FailedMutation>{});
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto failures = under_test->AsyncBulkApply(kTableName, Mutation());
  EXPECT_THAT(failures.get(), IsEmpty());

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncBulkApply"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.failed_mutations", 0),
                            SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.successful_mutations", 1)))));
}

TEST(DataTracingConnection, AsyncBulkApplyFailure) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncBulkApply).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future(std::vector<bigtable::FailedMutation>{
        {internal::AbortedError("fail"), 1},
        {internal::AbortedError("fail"), 2}});
  });

  bigtable::BulkMutation mut;
  for (auto i = 0; i != 10; ++i) mut.push_back(Mutation());
  auto under_test = MakeDataTracingConnection(mock);
  auto failures = under_test->AsyncBulkApply(kTableName, std::move(mut));
  EXPECT_THAT(failures.get(), SizeIs(2));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncBulkApply"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError),
          SpanHasAttributes(SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.failed_mutations", 2),
                            SpanAttribute<std::uint32_t>(
                                "gcloud.bigtable.successful_mutations", 8)))));
}

TEST(DataTracingConnection, ReadRows) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRows).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return bigtable_mocks::MakeRowReader({}, internal::AbortedError("fail"));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto reader = under_test->ReadRows(kTableName, bigtable::RowSet(), 0,
                                     bigtable::Filter::PassAllFilter());
  auto it = reader.begin();
  EXPECT_THAT(*it, StatusIs(StatusCode::kAborted));
  EXPECT_EQ(++it, reader.end());

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::ReadRows"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, ReadRowFound) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return std::pair<bool, bigtable::Row>{true, {"row", {}}};
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto row =
      under_test->ReadRow(kTableName, "row", bigtable::Filter::PassAllFilter());
  EXPECT_THAT(row, IsOkAndHolds(Pair(true, _)));

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("bigtable::Table::ReadRow"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasAttributes(SpanAttribute<int>("gcloud.status_code", 0),
                                    SpanAttribute<bool>(
                                        "gcloud.bigtable.row_found", true)))));
}

TEST(DataTracingConnection, ReadRowNotFound) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return std::pair<bool, bigtable::Row>{false, {"row", {}}};
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto row =
      under_test->ReadRow(kTableName, "row", bigtable::Filter::PassAllFilter());
  EXPECT_THAT(row, IsOkAndHolds(Pair(false, _)));

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("bigtable::Table::ReadRow"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasAttributes(SpanAttribute<int>("gcloud.status_code", 0),
                                    SpanAttribute<bool>(
                                        "gcloud.bigtable.row_found", false)))));
}

TEST(DataTracingConnection, ReadRowFailure) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto row =
      under_test->ReadRow(kTableName, "row", bigtable::Filter::PassAllFilter());
  EXPECT_THAT(row, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::ReadRow"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)),
          Not(SpanHasAttributes(
              SpanAttribute<bool>("gcloud.bigtable.row_found", _))))));
}

TEST(DataTracingConnection, CheckAndMutateRow) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, CheckAndMutateRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto branch = under_test->CheckAndMutateRow(
      kTableName, "row", bigtable::Filter::PassAllFilter(), {}, {});
  EXPECT_THAT(branch, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::CheckAndMutateRow"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, AsyncCheckAndMutateRow) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future<StatusOr<bigtable::MutationBranch>>(
        internal::AbortedError("fail"));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto branch = under_test->AsyncCheckAndMutateRow(
      kTableName, "row", bigtable::Filter::PassAllFilter(), {}, {});
  EXPECT_THAT(branch.get(), StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncCheckAndMutateRow"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, SampleRows) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, SampleRows).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto samples = under_test->SampleRows(kTableName);
  EXPECT_THAT(samples, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::SampleRows"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, AsyncSampleRows) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncSampleRows).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future<StatusOr<std::vector<bigtable::RowKeySample>>>(
        internal::AbortedError("fail"));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto samples = under_test->AsyncSampleRows(kTableName);
  EXPECT_THAT(samples.get(), StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncSampleRows"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, ReadModifyWriteRow) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, ReadModifyWriteRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return internal::AbortedError("fail");
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto samples = under_test->ReadModifyWriteRow({});
  EXPECT_THAT(samples, StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::ReadModifyWriteRow"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, AsyncReadModifyWriteRow) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future<StatusOr<bigtable::Row>>(
        internal::AbortedError("fail"));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto samples = under_test->AsyncReadModifyWriteRow({});
  EXPECT_THAT(samples.get(), StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncReadModifyWriteRow"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)))));
}

TEST(DataTracingConnection, AsyncReadRowFound) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future(
        make_status_or<std::pair<bool, bigtable::Row>>({true, {"row", {}}}));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto row = under_test->AsyncReadRow(kTableName, "row",
                                      bigtable::Filter::PassAllFilter());
  EXPECT_THAT(row.get(), IsOkAndHolds(Pair(true, _)));

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("bigtable::Table::AsyncReadRow"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasAttributes(SpanAttribute<int>("gcloud.status_code", 0),
                                    SpanAttribute<bool>(
                                        "gcloud.bigtable.row_found", true)))));
}

TEST(DataTracingConnection, AsyncReadRowNotFound) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future(
        make_status_or<std::pair<bool, bigtable::Row>>({false, {"row", {}}}));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto row = under_test->AsyncReadRow(kTableName, "row",
                                      bigtable::Filter::PassAllFilter());
  EXPECT_THAT(row.get(), IsOkAndHolds(Pair(false, _)));

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanHasInstrumentationScope(), SpanKindIsClient(),
                  SpanNamed("bigtable::Table::AsyncReadRow"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanHasAttributes(SpanAttribute<int>("gcloud.status_code", 0),
                                    SpanAttribute<bool>(
                                        "gcloud.bigtable.row_found", false)))));
}

TEST(DataTracingConnection, AsyncReadRowFailure) {
  auto span_catcher = InstallSpanCatcher();

  auto mock = std::make_shared<MockDataConnection>();
  EXPECT_CALL(*mock, AsyncReadRow).WillOnce([] {
    EXPECT_TRUE(ThereIsAnActiveSpan());
    return make_ready_future<StatusOr<std::pair<bool, bigtable::Row>>>(
        internal::AbortedError("fail"));
  });

  auto under_test = MakeDataTracingConnection(mock);
  auto row = under_test->AsyncReadRow(kTableName, "row",
                                      bigtable::Filter::PassAllFilter());
  EXPECT_THAT(row.get(), StatusIs(StatusCode::kAborted));

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanHasInstrumentationScope(), SpanKindIsClient(),
          SpanNamed("bigtable::Table::AsyncReadRow"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", kErrorCode)),
          Not(SpanHasAttributes(
              SpanAttribute<bool>("gcloud.bigtable.row_found", _))))));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
