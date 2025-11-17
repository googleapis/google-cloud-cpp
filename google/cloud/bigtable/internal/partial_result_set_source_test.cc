// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/partial_result_set_source.h"
#include "google/cloud/bigtable/mocks/mock_query_row.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/bigtable/testing/mock_partial_result_set_reader.h"
#include "google/cloud/bigtable/value.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#endif
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/substitute.h"
#include <google/bigtable/v2/data.pb.h>
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::Return;
using ::testing::UnitTest;

std::string CurrentTestName() {
  return UnitTest::GetInstance()->current_test_info()->name();
}

struct StringOption {
  using Type = std::string;
};

// Create the `PartialResultSetSource` within an `OptionsSpan` that has its
// `StringOption` set to the current test name, so that we might check that
// all `PartialResultSetReader` calls happen within a matching span.
StatusOr<std::unique_ptr<bigtable::ResultSourceInterface>>
CreatePartialResultSetSource(
    absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
    std::shared_ptr<OperationContext> operation_context,
    std::unique_ptr<PartialResultSetReader> reader, Options opts = {}) {
  internal::OptionsSpan span(internal::MergeOptions(
      std::move(opts.set<StringOption>(CurrentTestName())),
      internal::CurrentOptions()));
  return PartialResultSetSource::Create(
      std::move(metadata), std::move(operation_context), std::move(reader));
}

// Returns a functor that will return the argument after expecting that the
// current `StringOption` matches the test name.
template <typename T>
std::function<T()> ResultMock(T const& result) {
  return [result]() {
    EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
              CurrentTestName());
    return result;
  };
}

MATCHER_P(IsValidAndEquals, expected,
          "Verifies that a StatusOr<QueryRow> contains the given QueryRow") {
  return arg && *arg == expected;
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

/// @test Verify the behavior when the initial `Read()` fails.
TEST(PartialResultSetSourceTest, InitialReadFailure) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone)
      .WillOnce([](opentelemetry::context::Context const&,
                   OnDoneParams const& p) -> void {
        EXPECT_THAT(p.operation_status, StatusIs(StatusCode::kInvalidArgument));
      });
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

  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([](absl::optional<std::string> const&,
                   UnownedPartialResultSet&) { return false; });
  EXPECT_CALL(*grpc_reader, Finish())
      .WillOnce(ResultMock(Status(StatusCode::kInvalidArgument, "invalid")));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(
      absl::nullopt, std::move(operation_context), std::move(grpc_reader));
  EXPECT_THAT(reader, StatusIs(StatusCode::kInvalidArgument, "invalid"));
}

/**
 * @test Verify the behavior when the response does not contain data.
 */
TEST(PartialResultSetSourceTest, MissingRowTypeNoData) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone)
      .WillOnce([](opentelemetry::context::Context const&,
                   OnDoneParams const& p) -> void {
        EXPECT_THAT(p.operation_status, IsOk());
      });
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

  auto constexpr kText = R"pb(
    proto_rows_batch: {}
  )pb";
  google::bigtable::v2::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &response));

  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([&response](absl::optional<std::string> const&,
                            UnownedPartialResultSet& result) {
        result.result = response;
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);

  grpc::ClientContext context;
  EXPECT_CALL(*grpc_reader, context)
      .Times(2)
      .WillRepeatedly([&]() -> grpc::ClientContext const& { return context; });

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(
      google::bigtable::v2::ResultSetMetadata{}, std::move(operation_context),
      std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(bigtable::QueryRow{}));
}

/// @test Verify a single response is handled correctly.
TEST(PartialResultSetSourceTest, SingleResponse) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone)
      .WillOnce([](opentelemetry::context::Context const&,
                   OnDoneParams const& p) -> void {
        EXPECT_THAT(p.operation_status, IsOk());
      });
  EXPECT_CALL(*mock_metric, ElementRequest).Times(2);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(2);
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

  auto constexpr kResultMetadataText = R"pb(
    proto_schema {
      columns {
        name: "user_id"
        type { string_type {} }
      }
      columns {
        name: "email"
        type { string_type {} }
      }
      columns {
        name: "name"
        type { string_type {} }
      }
    }
  )pb";
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kResultMetadataText, &metadata));
  auto constexpr kProtoRowsText = R"pb(
    values { string_value: "r1" }
    values { string_value: "f1" }
    values { string_value: "q1" }
  )pb";
  google::bigtable::v2::ProtoRows proto_rows;
  ASSERT_TRUE(TextFormat::ParseFromString(kProtoRowsText, &proto_rows));

  std::string binary_batch_data = proto_rows.SerializeAsString();
  std::string partial_result_set_text =
      absl::Substitute(R"pb(
                         proto_rows_batch: {
                           batch_data: "$0",
                         },
                         resume_token: "AAAAAWVyZXN1bWVfdG9rZW4=",
                         reset: true,
                         estimated_batch_size: 31,
                         batch_checksum: 123456
                       )pb",
                       binary_batch_data);
  google::bigtable::v2::PartialResultSet response;
  ASSERT_TRUE(TextFormat::ParseFromString(partial_result_set_text, &response));

  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([&response](absl::optional<std::string> const&,
                            UnownedPartialResultSet& result) {
        result.result = response;
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);
  grpc::ClientContext context;
  EXPECT_CALL(*grpc_reader, context)
      .Times(4)
      .WillRepeatedly([&]() -> grpc::ClientContext const& { return context; });

  auto reader = CreatePartialResultSetSource(
      metadata, std::move(operation_context), std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);
  auto row = (*reader)->NextRow();
  ASSERT_STATUS_OK(row);
  auto first_row = *row;
  ASSERT_EQ(first_row.values().size(), 3);
  EXPECT_EQ(*row->values().at(0).get<std::string>(), "r1");
  EXPECT_EQ(*row->values().at(1).get<std::string>(), "f1");
  EXPECT_EQ(*row->values().at(2).get<std::string>(), "q1");

  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(bigtable::QueryRow{}));
}

/**
 * @test Verify the behavior when we get an incomplete Row.
 */
TEST(PartialResultSetSourceTest, MultipleResponses) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone)
      .WillOnce([](opentelemetry::context::Context const&,
                   OnDoneParams const& p) -> void {
        EXPECT_THAT(p.operation_status, IsOk());
      });
  EXPECT_CALL(*mock_metric, ElementRequest).Times(4);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(4);
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

  auto constexpr kResultMetadataText = R"pb(
    proto_schema {
      columns {
        name: "user_id"
        type { string_type {} }
      }
      columns {
        name: "email"
        type { string_type {} }
      }
      columns {
        name: "name"
        type { string_type {} }
      }
    }
  )pb";
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kResultMetadataText, &metadata));

  std::array<char const*, 3> proto_rows_text{{
      R"pb(
        values { string_value: "a1" }
        values { string_value: "a2" }
        values { string_value: "a3" }
      )pb",
      R"pb(
        values { string_value: "b1" }
        values { string_value: "b2" }
        values { string_value: "b3" }
      )pb",
      R"pb(
        values { string_value: "c1" }
        values { string_value: "c2" }
        values { string_value: "c3" }
      )pb",
  }};

  std::vector<google::bigtable::v2::PartialResultSet> responses;
  for (auto const* text : proto_rows_text) {
    google::bigtable::v2::ProtoRows proto_rows;
    ASSERT_TRUE(TextFormat::ParseFromString(text, &proto_rows));
    std::string binary_batch_data = proto_rows.SerializeAsString();
    std::string partial_result_set_text = absl::Substitute(
        R"pb(
          proto_rows_batch: {
            batch_data: "$0",
          },
          resume_token: "AAAAAWVyZXN1bWVfdG9rZW4=",
          reset: true,
          estimated_batch_size: 31,
          batch_checksum: 123456
        )pb",
        binary_batch_data);
    google::bigtable::v2::PartialResultSet response;
    ASSERT_TRUE(
        TextFormat::ParseFromString(partial_result_set_text, &response));
    responses.push_back(std::move(response));
  }

  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([&responses](absl::optional<std::string> const&,
                             UnownedPartialResultSet& result) {
        result.result = responses[0];
        return true;
      })
      .WillOnce([&responses](absl::optional<std::string> const&,
                             UnownedPartialResultSet& result) {
        result.result = responses[1];
        return true;
      })
      .WillOnce([&responses](absl::optional<std::string> const&,
                             UnownedPartialResultSet& result) {
        result.result = responses[2];
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);
  grpc::ClientContext context;
  EXPECT_CALL(*grpc_reader, context)
      .Times(8)
      .WillRepeatedly([&]() -> grpc::ClientContext const& { return context; });

  auto reader = CreatePartialResultSetSource(
      metadata, std::move(operation_context), std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  auto row0 = (*reader)->NextRow();
  ASSERT_STATUS_OK(row0);
  ASSERT_EQ(row0->values().size(), 3);
  EXPECT_EQ(*row0->values().at(0).get<std::string>(), "a1");
  EXPECT_EQ(*row0->values().at(1).get<std::string>(), "a2");
  EXPECT_EQ(*row0->values().at(2).get<std::string>(), "a3");

  auto row1 = (*reader)->NextRow();
  ASSERT_STATUS_OK(row1);
  ASSERT_EQ(row1->values().size(), 3);
  EXPECT_EQ(*row1->values().at(0).get<std::string>(), "b1");
  EXPECT_EQ(*row1->values().at(1).get<std::string>(), "b2");
  EXPECT_EQ(*row1->values().at(2).get<std::string>(), "b3");

  auto row2 = (*reader)->NextRow();
  ASSERT_STATUS_OK(row2);
  ASSERT_EQ(row2->values().size(), 3);
  EXPECT_EQ(*row2->values().at(0).get<std::string>(), "c1");
  EXPECT_EQ(*row2->values().at(1).get<std::string>(), "c2");
  EXPECT_EQ(*row2->values().at(2).get<std::string>(), "c3");

  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(bigtable::QueryRow{}));
}

/**
 * @test Verify the behavior when we get an incomplete Row.
 */
TEST(PartialResultSetSourceTest, ResponseWithNoValues) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone)
      .WillOnce([](opentelemetry::context::Context const&,
                   OnDoneParams const& p) -> void {
        EXPECT_THAT(p.operation_status, IsOk());
      });
  EXPECT_CALL(*mock_metric, ElementRequest).Times(2);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(2);
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

  auto constexpr kResultMetadataText = R"pb(
    proto_schema {
      columns {
        name: "user_id"
        type { string_type {} }
      }
    }
  )pb";
  google::bigtable::v2::ResultSetMetadata metadata;
  ASSERT_TRUE(TextFormat::ParseFromString(kResultMetadataText, &metadata));

  std::array<char const*, 3> proto_rows_text{{
      R"pb(
        values { string_value: "a1" }
      )pb",
      R"pb(
      )pb",
      R"pb(
      )pb"}};

  std::vector<google::bigtable::v2::PartialResultSet> responses;
  for (auto const* text : proto_rows_text) {
    google::bigtable::v2::ProtoRows proto_rows;
    ASSERT_TRUE(TextFormat::ParseFromString(text, &proto_rows));
    std::string binary_batch_data = proto_rows.SerializeAsString();
    std::string partial_result_set_text = absl::Substitute(
        R"pb(
          proto_rows_batch: {
            batch_data: "$0",
          },
          resume_token: "AAAAAWVyZXN1bWVfdG9rZW4=",
          reset: true,
          estimated_batch_size: 31,
          batch_checksum: 123456
        )pb",
        binary_batch_data);
    google::bigtable::v2::PartialResultSet response;
    ASSERT_TRUE(
        TextFormat::ParseFromString(partial_result_set_text, &response));
    responses.push_back(std::move(response));
  }

  auto grpc_reader =
      std::make_unique<bigtable_testing::MockPartialResultSetReader>();
  EXPECT_CALL(*grpc_reader, Read(_, _))
      .WillOnce([&responses](absl::optional<std::string> const&,
                             UnownedPartialResultSet& result) {
        result.result = responses[0];
        return true;
      })
      .WillOnce([&responses](absl::optional<std::string> const&,
                             UnownedPartialResultSet& result) {
        result.result = responses[1];
        return true;
      })
      .WillOnce([&responses](absl::optional<std::string> const&,
                             UnownedPartialResultSet& result) {
        result.result = responses[2];
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*grpc_reader, Finish()).WillOnce(ResultMock(Status()));
  EXPECT_CALL(*grpc_reader, TryCancel()).Times(0);
  grpc::ClientContext context;
  EXPECT_CALL(*grpc_reader, context)
      .Times(4)
      .WillRepeatedly([&]() -> grpc::ClientContext const& { return context; });

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto reader = CreatePartialResultSetSource(
      metadata, std::move(operation_context), std::move(grpc_reader));
  ASSERT_STATUS_OK(reader);

  // Verify the returned row is correct.
  EXPECT_THAT((*reader)->NextRow(),
              IsValidAndEquals(bigtable_mocks::MakeQueryRow(
                  {{"user_id", bigtable::Value("a1")}})));

  // At end of stream, we get an 'ok' response with an empty row.
  EXPECT_THAT((*reader)->NextRow(), IsValidAndEquals(bigtable::QueryRow{}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
