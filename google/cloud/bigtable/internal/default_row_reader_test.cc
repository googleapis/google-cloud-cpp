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

#include "google/cloud/bigtable/internal/default_row_reader.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/internal/make_status.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#endif
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::bigtable::v2::ReadRowsRequest;
using ::google::cloud::bigtable::DataLimitedErrorCountRetryPolicy;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockDataRetryPolicy;
using ::google::cloud::bigtable::testing::MockReadRowsStream;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ms = std::chrono::milliseconds;

auto constexpr kNumRetries = 2;
auto const* const kAppProfile = "the-profile";
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";

Matcher<ReadRowsRequest const&> HasCorrectResourceNames() {
  return AllOf(Property(&ReadRowsRequest::app_profile_id, Eq(kAppProfile)),
               Property(&ReadRowsRequest::table_name, Eq(kTableName)));
}

// Match the row limit in a request
Matcher<ReadRowsRequest const&> RequestWithRowsLimit(std::int64_t n) {
  return Property(&ReadRowsRequest::rows_limit, Eq(n));
}

google::bigtable::v2::ReadRowsResponse MakeRow(std::string row_key) {
  google::bigtable::v2::ReadRowsResponse response;
  auto& chunk = *response.add_chunks();
  *chunk.mutable_row_key() = std::move(row_key);
  chunk.mutable_family_name()->set_value("cf");
  chunk.mutable_qualifier()->set_value("cq");
  chunk.set_commit_row(true);
  return response;
}

google::bigtable::v2::ReadRowsResponse MalformedResponse() {
  // We return a malformed response that we know will fail in the parser, with
  // some INTERNAL error. This response fails because the column family is set,
  // but the column qualifier is not.
  google::bigtable::v2::ReadRowsResponse resp;
  auto& chunk = *resp.add_chunks();
  chunk.mutable_family_name()->set_value("cf");
  return resp;
}

std::vector<StatusOr<bigtable::RowKeyType>> StatusOrRowKeys(
    bigtable::RowReader& reader) {
  std::vector<StatusOr<bigtable::RowKeyType>> rows;
  for (auto& row : reader) {
    if (!row) {
      rows.emplace_back(std::move(row).status());
      continue;
    }
    rows.emplace_back(std::move(row->row_key()));
  }
  return rows;
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class MockMetric : public Metric {
 public:
  MOCK_METHOD(void, PreCall,
              (opentelemetry::context::Context const&, PreCallParams const&),
              (override));
  MOCK_METHOD(void, PostCall,
              (opentelemetry::context::Context const&,
               grpc::ClientContext const&, PostCallParams const&),
              (override));
  MOCK_METHOD(void, OnDone,
              (opentelemetry::context::Context const&, OnDoneParams const&),
              (override));
  MOCK_METHOD(void, ElementRequest,
              (opentelemetry::context::Context const&,
               ElementRequestParams const&),
              (override));
  MOCK_METHOD(void, ElementDelivery,
              (opentelemetry::context::Context const&,
               ElementDeliveryParams const&),
              (override));
  MOCK_METHOD(std::unique_ptr<Metric>, clone,
              (ResourceLabels resource_labels, DataLabels data_labels),
              (const, override));
};

// This class is a vehicle to get a MockMetric into the OperationContext object.
class CloningMetric : public Metric {
 public:
  explicit CloningMetric(std::unique_ptr<MockMetric> metric)
      : metric_(std::move(metric)) {}
  std::unique_ptr<Metric> clone(ResourceLabels, DataLabels) const override {
    return std::move(metric_);
  }

 private:
  mutable std::unique_ptr<MockMetric> metric_;
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class DefaultRowReaderTest : public ::testing::Test {
 protected:
  // Ensure that we set up the ClientContext once per stream
  Options TestOptions(int expected_streams) {
    EXPECT_CALL(mock_setup_, Call)
        .Times(expected_streams)
        .WillRepeatedly([this](grpc::ClientContext& context) {
          // We must manually populate the `grpc::ClientContext` with server
          // metadata, or else gRPC will assert. Using the `GrpcSetupOption` to
          // accomplish this is a bit of a hack.
          metadata_fixture_.SetServerMetadata(context);
        });
    return Options{}.set<internal::GrpcSetupOption>(
        mock_setup_.AsStdFunction());
  }

  DataLimitedErrorCountRetryPolicy retry_ =
      DataLimitedErrorCountRetryPolicy(kNumRetries);
  ExponentialBackoffPolicy backoff_ =
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0);
  MockFunction<void(grpc::ClientContext&)> mock_setup_;
  testing_util::ValidateMetadataFixture metadata_fixture_;
};

TEST_F(DefaultRowReaderTest, EmptyReaderHasNoRows) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), IsEmpty());
}

TEST_F(DefaultRowReaderTest, ReadOneRow) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, StreamIsDrained) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(MakeRow("r1")));
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Do not finish the iteration.  We still expect the stream to be finalized,
  // and the previously setup expectations on the mock `stream` check that.
}

TEST_F(DefaultRowReaderTest, RetryThenSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, NoRetryOnPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "fail")));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader),
              ElementsAre(StatusIs(StatusCode::kPermissionDenied)));
}

TEST_F(DefaultRowReaderTest, RetryPolicyExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](auto, auto const&,
                         google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/kNumRetries + 1));

  // Let's use a mock just to check that the backoff policy is used at all.
  auto backoff = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*backoff, OnCompletion)
      .Times(kNumRetries)
      .WillRepeatedly(Return(ms(10)));

  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call(ms(10))).Times(kNumRetries);

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), std::move(backoff), false,
      std::move(operation_context), mock_sleeper.AsStdFunction());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader),
              ElementsAre(StatusIs(StatusCode::kUnavailable)));
}

TEST_F(DefaultRowReaderTest, RetrySkipsAlreadyReadRows) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We should have two rows in the initial request: "r1" and "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We have read "r1". The new request should only contain: "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r2"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, RetrySkipsAlreadyScannedRows) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We start our call with 3 rows in the set: "r1", "r2", "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2", "r3"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            // Simulate the server returning an empty chunk with
            // `last_scanned_row_key` set to "r2".
            .WillOnce([]() {
              google::bigtable::v2::ReadRowsResponse resp;
              resp.set_last_scanned_row_key("r2");
              return resp;
            })
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We retry the remaining rows. We have "r1" returned, but the service
        // has also told us that "r2" was scanned. This means there is only one
        // row remaining to read: "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r3"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, FailedParseIsRetried) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(MalformedResponse()));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  // The parser only returns INTERNAL errors. Our default policies do not retry
  // on this StatusCode. We will use a mock policy to override this behavior.
  auto retry = std::make_unique<MockDataRetryPolicy>();
  EXPECT_CALL(*retry, OnFailure).WillOnce(Return(true));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, std::move(retry), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, FailedParseSkipsAlreadyReadRows) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We should have two rows in the initial request: "r1" and "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(MalformedResponse()));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We have read "r1". The new request should only contain: "r2".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r2"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  // The parser only returns INTERNAL errors. Our default policies do not retry
  // on this StatusCode. We will use a mock policy to override this behavior.
  auto retry = std::make_unique<MockDataRetryPolicy>();
  EXPECT_CALL(*retry, OnFailure).WillOnce(Return(true));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, std::move(retry), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, FailedParseSkipsAlreadyScannedRows) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We start our call with 3 rows in the set: "r1", "r2", "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2", "r3"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            // Simulate the server returning an empty chunk with
            // `last_scanned_row_key` set to "r2".
            .WillOnce([]() {
              google::bigtable::v2::ReadRowsResponse resp;
              resp.set_last_scanned_row_key("r2");
              return resp;
            })
            .WillOnce(Return(MalformedResponse()));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We retry the remaining rows. We have "r1" returned, but the service
        // has also told us that "r2" was scanned. This means there is only one
        // row remaining to read: "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r3"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  // The parser only returns INTERNAL errors. Our default policies do not retry
  // on this StatusCode. We will use a mock policy to override this behavior.
  auto retry = std::make_unique<MockDataRetryPolicy>();
  EXPECT_CALL(*retry, OnFailure).WillOnce(Return(true));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, std::move(retry), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, FailedParseWithPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(MalformedResponse()));
        // The stream is cancelled when the RowReader goes out of scope.
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader),
              ElementsAre(StatusIs(StatusCode::kInternal)));
}

TEST_F(DefaultRowReaderTest, NoRetryOnEmptyRowSet) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r2")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });
  // After receiving "r2", the row set will be empty. So even though we
  // encountered a transient error, there is no need to retry the stream.

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r2")));
}

TEST_F(DefaultRowReaderTest, RowLimitIsSent) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(42));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(), 42,
      bigtable::Filter::PassAllFilter(), false, retry_.clone(),
      backoff_.clone(), false, std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), IsEmpty());
}

TEST_F(DefaultRowReaderTest, RowLimitIsDecreasedOnRetry) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(42));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(41));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(), 42,
      bigtable::Filter::PassAllFilter(), false, retry_.clone(),
      backoff_.clone(), false, std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, NoRetryIfRowLimitReached) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(1));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });
  // After receiving "r1", the row set will be empty. So even though we
  // encountered a transient error, there is no need to retry the stream.

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(), 1,
      bigtable::Filter::PassAllFilter(), false, retry_.clone(),
      backoff_.clone(), false, std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, CancelDrainsStream) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(MakeRow("r1")));
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Manually cancel the call.
  reader.Cancel();
  it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(it->status().error_info().metadata(),
              Contains(Pair("gl-cpp.error.origin", "client")));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, CancelBeforeBegin) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone).Times(0);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows).Times(0);

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  // Manually cancel the call before a stream was created.
  reader.Cancel();
  reader.begin();

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(it->status().error_info().metadata(),
              Contains(Pair("gl-cpp.error.origin", "client")));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, RowReaderConstructorDoesNotCallRpc) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone).Times(0);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  // The RowReader constructor/destructor by themselves should not
  // invoke the RPC or create parsers (the latter restriction because
  // parsers are per-connection and non-reusable).
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows).Times(0);

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
}

TEST_F(DefaultRowReaderTest, RetryUsesNewContext) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](auto context, auto const&,
                         google::bigtable::v2::ReadRowsRequest const& request) {
        // This is a hack. A new request will have the default compression
        // algorithm (GRPC_COMPRESS_NONE). We then change the value in this
        // call. If the context is reused, it will no longer have the default
        // value.
        EXPECT_EQ(GRPC_COMPRESS_NONE, context->compression_algorithm());
        context->set_compression_algorithm(GRPC_COMPRESS_GZIP);

        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/kNumRetries + 1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader),
              ElementsAre(StatusIs(StatusCode::kUnavailable)));
}

TEST_F(DefaultRowReaderTest, ReverseScanSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(3);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(3);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        auto stream = std::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r3")))
            .WillOnce(Return(MakeRow("r2")))
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      true, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(
      StatusOrRowKeys(reader),
      ElementsAre(IsOkAndHolds("r3"), IsOkAndHolds("r2"), IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, ReverseScanFailsOnIncreasingRowKeyOrder) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        auto stream = std::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(MakeRow("r2")));
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      true, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(
      StatusOrRowKeys(reader),
      ElementsAre(
          IsOkAndHolds("r1"),
          StatusIs(StatusCode::kInternal,
                   HasSubstr("keys are expected in decreasing order"))));
}

TEST_F(DefaultRowReaderTest, ReverseScanResumption) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        // We start our call with 3 rows in the set: "r1", "r2", "r3".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1", "r2", "r3"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r3")))
            // Simulate the server returning an empty chunk with
            // `last_scanned_row_key` set to "r2".
            .WillOnce([]() {
              google::bigtable::v2::ReadRowsResponse resp;
              resp.set_last_scanned_row_key("r2");
              return resp;
            })
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        // We retry the remaining rows. We have "r3" returned, but the service
        // has also told us that "r2" was scanned. This means there is only one
        // row remaining to read: "r1".
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("r1"));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      true, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r3")));
}

TEST_F(DefaultRowReaderTest, BigtableCookies) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::ReadRowsRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(internal::UnavailableError("try again")));
        return stream;
      })
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::ReadRowsRequest const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(*context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(internal::PermissionDeniedError("fail")));
        return stream;
      });

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      true, retry_.clone(), backoff_.clone(), false,
      std::make_shared<OperationContext>());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader),
              ElementsAre(StatusIs(StatusCode::kPermissionDenied)));
}

TEST_F(DefaultRowReaderTest, RetryInfoHeeded) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto const delay = ms(std::chrono::minutes(5));
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([delay](auto, auto const&,
                        google::bigtable::v2::ReadRowsRequest const&) {
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce([delay] {
          auto s = internal::ResourceExhaustedError("try again");
          internal::SetRetryInfo(s, internal::RetryInfo{delay});
          return s;
        });
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call(delay));

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), true,
      std::move(operation_context), mock_sleeper.AsStdFunction());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader), ElementsAre(IsOkAndHolds("r1")));
}

TEST_F(DefaultRowReaderTest, RetryInfoIgnored) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto const delay = ms(std::chrono::minutes(5));
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([delay](auto, auto const&,
                        google::bigtable::v2::ReadRowsRequest const&) {
        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce([delay] {
          auto s = internal::ResourceExhaustedError("try again");
          internal::SetRetryInfo(s, internal::RetryInfo{delay});
          return s;
        });
        return stream;
      });

  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call).Times(0);

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      false, retry_.clone(), backoff_.clone(), false,
      std::move(operation_context), mock_sleeper.AsStdFunction());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
  EXPECT_THAT(StatusOrRowKeys(reader),
              ElementsAre(StatusIs(StatusCode::kResourceExhausted)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
