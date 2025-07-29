// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/data_connection.h"
#include "google/cloud/bigtable/internal/defaults.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#endif
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_limiter.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ::google::cloud::bigtable::AppProfileIdOption;
using ::google::cloud::bigtable::DataBackoffPolicyOption;
using ::google::cloud::bigtable::DataLimitedErrorCountRetryPolicy;
using ::google::cloud::bigtable::DataRetryPolicyOption;
using ::google::cloud::bigtable::IdempotentMutationPolicyOption;
using ::google::cloud::bigtable::ReverseScanOption;
using ::google::cloud::bigtable::testing::MockAsyncReadRowsStream;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockIdempotentMutationPolicy;
using ::google::cloud::bigtable::testing::MockMutateRowsLimiter;
using ::google::cloud::bigtable::testing::MockMutateRowsStream;
using ::google::cloud::bigtable::testing::MockReadRowsStream;
using ::google::cloud::bigtable::testing::MockSampleRowKeysStream;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::An;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ms = std::chrono::milliseconds;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try again");
}

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "fail");
}

bigtable::SingleRowMutation IdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(
      row_key, {bigtable::SetCell("fam", "col", ms(0), "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(row_key,
                                     {bigtable::SetCell("fam", "col", "val")});
}

Matcher<v2::MutateRowsRequest::Entry> Entry(std::string const& row_key) {
  return Property(&v2::MutateRowsRequest::Entry::row_key, row_key);
}

void CheckFailedMutations(
    std::vector<bigtable::FailedMutation> const& actual,
    std::vector<bigtable::FailedMutation> const& expected) {
  struct Unroll {
    explicit Unroll(std::vector<bigtable::FailedMutation> const& failed) {
      for (auto const& f : failed) {
        statuses.push_back(f.status().code());
        indices.push_back(f.original_index());
      }
    }
    std::vector<StatusCode> statuses;
    std::vector<int> indices;
  };

  auto a = Unroll(actual);
  auto e = Unroll(expected);
  EXPECT_THAT(a.statuses, ElementsAreArray(e.statuses));
  EXPECT_THAT(a.indices, ElementsAreArray(e.indices));
}

// Individual entry pairs are: {index, StatusCode}
v2::MutateRowsResponse MakeBulkApplyResponse(
    std::vector<std::pair<int, grpc::StatusCode>> const& entries) {
  v2::MutateRowsResponse resp;
  for (auto entry : entries) {
    auto& e = *resp.add_entries();
    e.set_index(entry.first);
    e.mutable_status()->set_code(entry.second);
  }
  return resp;
}

bigtable::RowSet TestRowSet() { return bigtable::RowSet("r1", "r2"); }

Matcher<v2::ReadRowsRequest const&> HasTestRowSet() {
  return Property(&v2::ReadRowsRequest::rows,
                  Property(&v2::RowSet::row_keys, ElementsAre("r1", "r2")));
}

bigtable::Filter TestFilter() { return bigtable::Filter::Latest(5); }

Matcher<v2::RowFilter const&> IsTestFilter() {
  return Property(&v2::RowFilter::cells_per_column_limit_filter, Eq(5));
}

Matcher<v2::Mutation const&> MatchMutation(bigtable::Mutation const& m) {
  // For simplicity, we only use SetCell mutations in these tests.
  auto const& s = m.op.set_cell();
  return Property(
      &v2::Mutation::set_cell,
      AllOf(Property(&v2::Mutation::SetCell::family_name, s.family_name()),
            Property(&v2::Mutation::SetCell::column_qualifier,
                     s.column_qualifier()),
            Property(&v2::Mutation::SetCell::timestamp_micros,
                     s.timestamp_micros()),
            Property(&v2::Mutation::SetCell::value, s.value())));
}

Matcher<bigtable::Cell const&> MatchCell(bigtable::Cell const& c) {
  return AllOf(
      Property(&bigtable::Cell::row_key, c.row_key()),
      Property(&bigtable::Cell::family_name, c.family_name()),
      Property(&bigtable::Cell::column_qualifier, c.column_qualifier()),
      Property(&bigtable::Cell::timestamp, c.timestamp()),
      Property(&bigtable::Cell::value, c.value()),
      Property(&bigtable::Cell::labels, ElementsAreArray(c.labels())));
}

v2::SampleRowKeysResponse MakeSampleRowsResponse(std::string row_key,
                                                 std::int64_t offset) {
  v2::SampleRowKeysResponse r;
  r.set_row_key(std::move(row_key));
  r.set_offset_bytes(offset);
  return r;
};

struct RowKeySampleVectors {
  explicit RowKeySampleVectors(std::vector<bigtable::RowKeySample> samples) {
    row_keys.reserve(samples.size());
    offset_bytes.reserve(samples.size());
    for (auto& sample : samples) {
      row_keys.emplace_back(std::move(sample.row_key));
      offset_bytes.emplace_back(std::move(sample.offset_bytes));
    }
  }

  std::vector<std::string> row_keys;
  std::vector<std::int64_t> offset_bytes;
};

DataLimitedErrorCountRetryPolicy TestRetryPolicy() {
  return DataLimitedErrorCountRetryPolicy(kNumRetries);
}

ExponentialBackoffPolicy TestBackoffPolicy() {
  return ExponentialBackoffPolicy(ms(0), ms(0), 2.0);
}

// For tests that need to manually interact with the client/server metadata in a
// `grpc::ClientContext`.
Options CallOptionsWithoutClientContextSetup() {
  return bigtable::internal::DefaultDataOptions(
      Options{}
          .set<bigtable::AppProfileIdOption>(kAppProfile)
          .set<DataRetryPolicyOption>(TestRetryPolicy().clone())
          .set<DataBackoffPolicyOption>(TestBackoffPolicy().clone()));
}

std::shared_ptr<DataConnectionImpl> TestConnection(
    std::shared_ptr<BigtableStub> stub,
    std::shared_ptr<MutateRowsLimiter> limiter =
        std::make_shared<NoopMutateRowsLimiter>()) {
  auto background = internal::MakeBackgroundThreadsFactory()();
  return std::make_shared<DataConnectionImpl>(
      std::move(background), std::move(stub), std::move(limiter), Options{});
}

std::shared_ptr<DataConnectionImpl> TestConnection(
    std::shared_ptr<BigtableStub> stub,
    std::unique_ptr<OperationContextFactory> operation_context_factory,
    std::shared_ptr<MutateRowsLimiter> limiter =
        std::make_shared<NoopMutateRowsLimiter>()) {
  auto background = internal::MakeBackgroundThreadsFactory()();
  return std::make_shared<DataConnectionImpl>(
      std::move(background), std::move(stub),
      std::move(operation_context_factory), std::move(limiter), Options{});
}

TEST(TransformReadModifyWriteRowResponse, Basic) {
  v2::ReadModifyWriteRowResponse response;
  auto constexpr kText = R"pb(
    row {
      key: "row"
      families {
        name: "cf1"
        columns {
          qualifier: "cq1"
          cells { value: "100" }
          cells { value: "200" }
        }
      }
      families {
        name: "cf2"
        columns {
          qualifier: "cq2"
          cells { value: "with-timestamp" timestamp_micros: 10 }
        }
        columns {
          qualifier: "cq3"
          cells { value: "with-labels" labels: "l1" labels: "l2" }
        }
      }
    })pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  auto row = TransformReadModifyWriteRowResponse(std::move(response));
  EXPECT_EQ("row", row.row_key());

  auto c1 = bigtable::Cell("row", "cf1", "cq1", 0, "100");
  auto c2 = bigtable::Cell("row", "cf1", "cq1", 0, "200");
  auto c3 = bigtable::Cell("row", "cf2", "cq2", 10, "with-timestamp");
  auto c4 = bigtable::Cell("row", "cf2", "cq3", 0, "with-labels", {"l1", "l2"});
  EXPECT_THAT(row.cells(), ElementsAre(MatchCell(c1), MatchCell(c2),
                                       MatchCell(c3), MatchCell(c4)));
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
  explicit CloningMetric(std::unique_ptr<MockMetric> metric) {
    metrics_.push_back(std::move(metric));
  }
  explicit CloningMetric(std::vector<std::unique_ptr<MockMetric>> metrics)
      : metrics_(std::move(metrics)) {
    std::reverse(metrics_.begin(), metrics_.end());
  }

  std::unique_ptr<Metric> clone(ResourceLabels, DataLabels) const override {
    auto m = std::move(metrics_.back());
    metrics_.pop_back();
    return m;
  }

 private:
  mutable std::vector<std::unique_ptr<MockMetric>> metrics_;
};

class FakeOperationContextFactory : public OperationContextFactory {
 public:
  FakeOperationContextFactory(ResourceLabels r, DataLabels d,
                              std::shared_ptr<Metric> metric,
                              std::shared_ptr<OperationContext::Clock> clock)
      : resource_labels_(std::move(r)),
        data_labels_(std::move(d)),
        clock_(std::move(clock)) {
    metrics_.push_back(std::move(metric));
  }

  std::shared_ptr<OperationContext> ReadRow(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }
  std::shared_ptr<OperationContext> ReadRows(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }
  std::shared_ptr<OperationContext> MutateRow(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }
  std::shared_ptr<OperationContext> MutateRows(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }
  std::shared_ptr<OperationContext> CheckAndMutateRow(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }
  std::shared_ptr<OperationContext> SampleRowKeys(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }
  std::shared_ptr<OperationContext> ReadModifyWriteRow(
      std::string const& name, std::string const& app_profile) override {
    return Helper(name, app_profile);
  }

 private:
  std::shared_ptr<OperationContext> Helper(std::string const& name,
                                           std::string const& app_profile) {
    resource_labels_.table = name;
    data_labels_.app_profile = app_profile;
    return std::make_shared<OperationContext>(resource_labels_, data_labels_,
                                              metrics_, clock_);
  }

  ResourceLabels resource_labels_;
  DataLabels data_labels_;
  std::vector<std::shared_ptr<Metric const>> metrics_;
  std::shared_ptr<OperationContext::Clock> clock_;
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class DataConnectionTest : public ::testing::Test {
 protected:
  Options CallOptions() {
    auto options = CallOptionsWithoutClientContextSetup();
    // All code paths try to access the server metadata returned in the
    // `grpc::ClientContext`. As a hack to get around some asserts in gRPC,
    // make sure that these `grpc::ClientContext`s have server metadata.
    auto setup = [this](grpc::ClientContext& context) {
      metadata_fixture_.SetServerMetadata(context, {});
    };
    return options.set<internal::GrpcSetupOption>(setup);
  }

  testing_util::ValidateMetadataFixture metadata_fixture_;
};

TEST_F(DataConnectionTest, ApplySuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return v2::MutateRowResponse{};
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->Apply(kTableName, IdempotentMutation("row"));
  ASSERT_STATUS_OK(status);
}

TEST_F(DataConnectionTest, ApplyPermanentFailure) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return PermanentError();
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->Apply(kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, ApplyRetryThenSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      })
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return v2::MutateRowResponse{};
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->Apply(kTableName, IdempotentMutation("row"));
  ASSERT_STATUS_OK(status);
}

TEST_F(DataConnectionTest, ApplyRetryExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](grpc::ClientContext&, Options const&,
                         v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto status = conn->Apply(kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, ApplyRetryIdempotency) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  auto mock_i = std::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = std::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone, is_idempotent(An<v2::Mutation const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto status = conn->Apply(kTableName, NonIdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, ApplyBigtableCookie) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRow)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::MutateRowRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        return TransientError();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::MutateRowRequest const&) {
        // Verify that the next request includes the bigtable cookie from
        // above.
        auto headers = metadata_fixture_.GetMetadata(context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        return PermanentError();
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(1);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptionsWithoutClientContextSetup().set<DataBackoffPolicyOption>(
          std::move(mock_b)));
  auto status = conn->Apply(kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, AsyncApplySuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([](google::cloud::CompletionQueue&, auto, auto,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future(make_status_or(v2::MutateRowResponse{}));
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->AsyncApply(kTableName, IdempotentMutation("row"));
  ASSERT_STATUS_OK(status.get());
}

TEST_F(DataConnectionTest, AsyncApplyPermanentFailure) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([](google::cloud::CompletionQueue&, auto, auto,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            PermanentError());
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->AsyncApply(kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, AsyncApplyRetryExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](google::cloud::CompletionQueue&, auto, auto,
                         v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            TransientError());
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto status = conn->AsyncApply(kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, AsyncApplyRetryIdempotency) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([](google::cloud::CompletionQueue&, auto, auto,
                   v2::MutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            TransientError());
      });

  auto mock_i = std::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = std::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone, is_idempotent(An<v2::Mutation const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto status = conn->AsyncApply(kTableName, NonIdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, AsyncApplyBigtableCookie) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRow)
      .WillOnce([this](CompletionQueue&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       auto, v2::MutateRowRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            TransientError());
      })
      .WillOnce([this](CompletionQueue&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       auto, v2::MutateRowRequest const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(*context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        return make_ready_future<StatusOr<v2::MutateRowResponse>>(
            PermanentError());
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(1);
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptionsWithoutClientContextSetup().set<DataBackoffPolicyOption>(
          std::move(mock_b)));
  auto status = conn->AsyncApply(kTableName, IdempotentMutation("row"));
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, BulkApplyEmpty) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone).Times(0);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  auto conn = TestConnection(std::move(mock), std::move(factory));
  auto actual = conn->BulkApply(kTableName, bigtable::BulkMutation());
  CheckFailedMutations(actual, {});
}

TEST_F(DataConnectionTest, BulkApplySuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(Entry("r0"), Entry("r1")));
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeBulkApplyResponse(
                {{0, grpc::StatusCode::OK}, {1, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, {});
}

TEST_F(DataConnectionTest, BulkApplyRetryMutationPolicy) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 2},
                                                    {TransientError(), 3}};
  bigtable::BulkMutation mut(
      IdempotentMutation("success"),
      IdempotentMutation("retries-transient-error"),
      IdempotentMutation("fails-with-permanent-error"),
      NonIdempotentMutation("fails-with-transient-error"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(
                MakeBulkApplyResponse({{0, grpc::StatusCode::OK},
                                       {1, grpc::StatusCode::UNAVAILABLE},
                                       {2, grpc::StatusCode::PERMISSION_DENIED},
                                       {3, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(Entry("retries-transient-error")));
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST_F(DataConnectionTest, BulkApplyIncompleteStreamRetried) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  bigtable::BulkMutation mut(IdempotentMutation("returned"),
                             IdempotentMutation("forgotten"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(Entry("forgotten")));
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, {});
}

TEST_F(DataConnectionTest, BulkApplyStreamRetryExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [](auto, auto const&,
             google::bigtable::v2::MutateRowsRequest const& request) {
            EXPECT_EQ(kAppProfile, request.app_profile_id());
            EXPECT_EQ(kTableName, request.table_name());
            auto stream = std::make_unique<MockMutateRowsStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(TransientError()));
            return stream;
          });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST_F(DataConnectionTest, BulkApplyStreamPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST_F(DataConnectionTest, BulkApplyNoSleepIfNoPendingMutations) {
  bigtable::BulkMutation mut(IdempotentMutation("succeeds"),
                             IdempotentMutation("fails-immediately"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeBulkApplyResponse(
                {{0, grpc::StatusCode::OK},
                 {1, grpc::StatusCode::PERMISSION_DENIED}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).Times(0);

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<DataBackoffPolicyOption>(std::move(mock_b)));
  (void)conn->BulkApply(kTableName, std::move(mut));
}

TEST_F(DataConnectionTest, BulkApplyRetriesOkStreamWithFailedMutations) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kUnavailable, "try again"), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [](auto, auto const&,
             google::bigtable::v2::MutateRowsRequest const& request) {
            EXPECT_EQ(kAppProfile, request.app_profile_id());
            EXPECT_EQ(kTableName, request.table_name());
            auto stream = std::make_unique<MockMutateRowsStream>();
            // The overall stream succeeds, but it contains failed mutations.
            // Our retry and backoff policies should take effect.
            EXPECT_CALL(*stream, Read)
                .WillOnce(Return(MakeBulkApplyResponse(
                    {{0, grpc::StatusCode::UNAVAILABLE}})))
                .WillOnce(Return(Status()));
            return stream;
          });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST_F(DataConnectionTest, BulkApplyRetryInfoHeeded) {
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&, v2::MutateRowsRequest const&) {
        auto status = PermanentError();
        internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(status));
        return stream;
      })
      .WillOnce([](auto, auto const&, v2::MutateRowsRequest const&) {
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeBulkApplyResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<EnableServerRetriesOption>(true));
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, {});
}

TEST_F(DataConnectionTest, BulkApplyRetryInfoIgnored) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](auto, auto const&, v2::MutateRowsRequest const&) {
        auto status = PermanentError();
        internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(status));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<EnableServerRetriesOption>(false));
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

TEST_F(DataConnectionTest, BulkApplyThrottling) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock_limiter = std::make_shared<MockMutateRowsLimiter>();
  auto mock_stub = std::make_shared<MockBigtableStub>();
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_limiter, Acquire);
    EXPECT_CALL(*mock_stub, MutateRows).WillOnce([] {
      auto stream = std::make_unique<MockMutateRowsStream>();
      EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
      return stream;
    });
  }

  auto conn = TestConnection(std::move(mock_stub), std::move(mock_limiter));
  internal::OptionsSpan span(CallOptions());
  auto actual = conn->BulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual, expected);
}

// The `AsyncBulkApplier` is tested extensively in `async_bulk_apply_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST_F(DataConnectionTest, AsyncBulkApply) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("row"));

  auto mock_limiter = std::make_shared<MockMutateRowsLimiter>();
  auto mock_stub = std::make_shared<MockBigtableStub>();
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_limiter, AsyncAcquire)
        .WillOnce(Return(ByMove(make_ready_future())));
    EXPECT_CALL(*mock_stub, AsyncMutateRows)
        .WillOnce([](CompletionQueue const&, auto, auto,
                     v2::MutateRowsRequest const& request) {
          EXPECT_EQ(kAppProfile, request.app_profile_id());
          EXPECT_EQ(kTableName, request.table_name());
          using ErrorStream =
              internal::AsyncStreamingReadRpcError<v2::MutateRowsResponse>;
          return std::make_unique<ErrorStream>(PermanentError());
        });
  }

  auto conn = TestConnection(std::move(mock_stub), std::move(mock_limiter));
  internal::OptionsSpan span(CallOptions());
  auto actual = conn->AsyncBulkApply(kTableName, std::move(mut));
  CheckFailedMutations(actual.get(), expected);
}

// The DefaultRowReader is tested extensively in `default_row_reader_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST_F(DataConnectionTest, ReadRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(42, request.rows_limit());
        EXPECT_THAT(request, HasTestRowSet());
        EXPECT_THAT(request.filter(), IsTestFilter());
        EXPECT_FALSE(request.reversed());

        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions());
  auto reader = conn->ReadRows(kTableName, TestRowSet(), 42, TestFilter());
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(DataConnectionTest, ReadRowsReverseScan) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());

        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions().set<ReverseScanOption>(true));
  auto reader = conn->ReadRows(kTableName, TestRowSet(), 42, TestFilter());
  EXPECT_EQ(reader.begin(), reader.end());
}

// The DefaultRowReader is tested extensively in `default_row_reader_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST_F(DataConnectionTest, ReadRowsFull) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(42, request.rows_limit());
        EXPECT_THAT(request, HasTestRowSet());
        EXPECT_THAT(request.filter(), IsTestFilter());
        EXPECT_TRUE(request.reversed());

        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions());
  auto reader = conn->ReadRowsFull(bigtable::ReadRowsParams{
      kTableName, kAppProfile, TestRowSet(), 42, TestFilter(), true});
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(DataConnectionTest, ReadRowsRetryInfoHeeded) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce(
          [](auto, auto const&, google::bigtable::v2::ReadRowsRequest const&) {
            auto status = PermanentError();
            internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
            auto stream = std::make_unique<MockReadRowsStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(status));
            return stream;
          })
      .WillOnce(
          [](auto, auto const&, google::bigtable::v2::ReadRowsRequest const&) {
            auto stream = std::make_unique<MockReadRowsStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
            return stream;
          });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<EnableServerRetriesOption>(true));
  auto reader = conn->ReadRowsFull(bigtable::ReadRowsParams{
      kTableName, kAppProfile, TestRowSet(), 42, TestFilter(), true});
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(DataConnectionTest, ReadRowsRetryInfoIgnored) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce(
          [](auto, auto const&, google::bigtable::v2::ReadRowsRequest const&) {
            auto status = PermanentError();
            internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
            auto stream = std::make_unique<MockReadRowsStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(status));
            return stream;
          });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<EnableServerRetriesOption>(false));
  auto reader = conn->ReadRowsFull(bigtable::ReadRowsParams{
      kTableName, kAppProfile, TestRowSet(), 42, TestFilter(), true});
  std::vector<StatusOr<bigtable::Row>> rows;
  for (auto const& row : reader) rows.push_back(row);
  EXPECT_THAT(rows, ElementsAre(StatusIs(PermanentError().code())));
}

TEST_F(DataConnectionTest, ReadRowEmpty) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto resp = conn->ReadRow(kTableName, "row", TestFilter());
  ASSERT_STATUS_OK(resp);
  EXPECT_FALSE(resp->first);
}

TEST_F(DataConnectionTest, ReadRowSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(1);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce([]() {
              v2::ReadRowsResponse r;
              auto& chunk = *r.add_chunks();
              *chunk.mutable_row_key() = "row";
              chunk.mutable_family_name()->set_value("cf");
              chunk.mutable_qualifier()->set_value("cq");
              chunk.set_commit_row(true);
              return r;
            })
            .WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto resp = conn->ReadRow(kTableName, "row", TestFilter());
  ASSERT_STATUS_OK(resp);
  EXPECT_TRUE(resp->first);
  EXPECT_EQ("row", resp->second.row_key());
}

TEST_F(DataConnectionTest, ReadRowFailure) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](auto, auto const&,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = std::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto resp = conn->ReadRow(kTableName, "row", TestFilter());
  EXPECT_THAT(resp, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, CheckAndMutateRowSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric1 = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric1, PreCall).Times(1);
  EXPECT_CALL(*mock_metric1, PostCall).Times(1);
  EXPECT_CALL(*mock_metric1, OnDone).Times(1);
  EXPECT_CALL(*mock_metric1, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric1, ElementDelivery).Times(0);
  auto mock_metric2 = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric2, PreCall).Times(1);
  EXPECT_CALL(*mock_metric2, PostCall).Times(1);
  EXPECT_CALL(*mock_metric2, OnDone).Times(1);
  EXPECT_CALL(*mock_metric2, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric2, ElementDelivery).Times(0);
  std::vector<std::unique_ptr<MockMetric>> v;
  v.push_back(std::move(mock_metric1));
  v.push_back(std::move(mock_metric2));
  auto fake_metric = std::make_shared<CloningMetric>(std::move(v));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([&](grpc::ClientContext&, Options const&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(true);
        return resp;
      })
      .WillOnce([&](grpc::ClientContext&, Options const&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(false);
        return resp;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto predicate = conn->CheckAndMutateRow(kTableName, "row", TestFilter(),
                                           {t1, t2}, {f1, f2});
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateMatched);

  predicate = conn->CheckAndMutateRow(kTableName, "row", TestFilter(), {t1, t2},
                                      {f1, f2});
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateNotMatched);
}

TEST_F(DataConnectionTest, CheckAndMutateRowIdempotency) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([&](grpc::ClientContext&, Options const&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return TransientError();
      });

  auto mock_i = std::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = std::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone,
                is_idempotent(An<v2::CheckAndMutateRowRequest const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto status = conn->CheckAndMutateRow(kTableName, "row", TestFilter(),
                                        {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, CheckAndMutateRowPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([&](grpc::ClientContext&, Options const&,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return PermanentError();
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->CheckAndMutateRow(kTableName, "row", TestFilter(),
                                        {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, CheckAndMutateRowRetryExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([&](grpc::ClientContext&, Options const&,
                          v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return TransientError();
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions()
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<IdempotentMutationPolicyOption>(
              bigtable::AlwaysRetryMutationPolicy().clone()));
  auto status = conn->CheckAndMutateRow(kTableName, "row", TestFilter(),
                                        {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, CheckAndMutateRowBigtableCookie) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, CheckAndMutateRow)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::CheckAndMutateRowRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        return TransientError();
      })
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::CheckAndMutateRowRequest const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        return PermanentError();
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(1);
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptionsWithoutClientContextSetup()
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<IdempotentMutationPolicyOption>(
              bigtable::AlwaysRetryMutationPolicy().clone()));
  auto status = conn->CheckAndMutateRow(kTableName, "row", TestFilter(),
                                        {t1, t2}, {f1, f2});
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, AsyncCheckAndMutateRowSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric1 = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric1, PreCall).Times(1);
  EXPECT_CALL(*mock_metric1, PostCall).Times(1);
  EXPECT_CALL(*mock_metric1, OnDone).Times(1);
  EXPECT_CALL(*mock_metric1, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric1, ElementDelivery).Times(0);
  auto mock_metric2 = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric2, PreCall).Times(1);
  EXPECT_CALL(*mock_metric2, PostCall).Times(1);
  EXPECT_CALL(*mock_metric2, OnDone).Times(1);
  EXPECT_CALL(*mock_metric2, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric2, ElementDelivery).Times(0);
  std::vector<std::unique_ptr<MockMetric>> v;
  v.push_back(std::move(mock_metric1));
  v.push_back(std::move(mock_metric2));
  auto fake_metric = std::make_shared<CloningMetric>(std::move(v));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([&](google::cloud::CompletionQueue&, auto, auto,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(true);
        return make_ready_future(make_status_or(resp));
      })
      .WillOnce([&](google::cloud::CompletionQueue&, auto, auto,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));

        v2::CheckAndMutateRowResponse resp;
        resp.set_predicate_matched(false);
        return make_ready_future(make_status_or(resp));
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto predicate = conn->AsyncCheckAndMutateRow(kTableName, "row", TestFilter(),
                                                {t1, t2}, {f1, f2})
                       .get();
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateMatched);

  predicate = conn->AsyncCheckAndMutateRow(kTableName, "row", TestFilter(),
                                           {t1, t2}, {f1, f2})
                  .get();
  ASSERT_STATUS_OK(predicate);
  EXPECT_EQ(*predicate, bigtable::MutationBranch::kPredicateNotMatched);
}

TEST_F(DataConnectionTest, AsyncCheckAndMutateRowIdempotency) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([&](google::cloud::CompletionQueue&, auto, auto,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            TransientError());
      });

  auto mock_i = std::make_unique<MockIdempotentMutationPolicy>();
  EXPECT_CALL(*mock_i, clone).WillOnce([]() {
    auto clone = std::make_unique<MockIdempotentMutationPolicy>();
    EXPECT_CALL(*clone,
                is_idempotent(An<v2::CheckAndMutateRowRequest const&>()))
        .WillOnce(Return(false));
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<IdempotentMutationPolicyOption>(std::move(mock_i)));
  auto status = conn->AsyncCheckAndMutateRow(kTableName, "row", TestFilter(),
                                             {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, AsyncCheckAndMutateRowPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([&](google::cloud::CompletionQueue&, auto, auto,
                    v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            PermanentError());
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto status = conn->AsyncCheckAndMutateRow(kTableName, "row", TestFilter(),
                                             {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, AsyncCheckAndMutateRowRetryExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .Times(kNumRetries + 1)
      .WillRepeatedly([&](google::cloud::CompletionQueue&, auto, auto,
                          v2::CheckAndMutateRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        EXPECT_THAT(request.predicate_filter(), IsTestFilter());
        EXPECT_THAT(request.true_mutations(),
                    ElementsAre(MatchMutation(t1), MatchMutation(t2)));
        EXPECT_THAT(request.false_mutations(),
                    ElementsAre(MatchMutation(f1), MatchMutation(f2)));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            TransientError());
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions()
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<IdempotentMutationPolicyOption>(
              bigtable::AlwaysRetryMutationPolicy().clone()));
  auto status = conn->AsyncCheckAndMutateRow(kTableName, "row", TestFilter(),
                                             {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, AsyncCheckAndMutateRowBigtableCookie) {
  auto t1 = bigtable::SetCell("f1", "c1", ms(0), "true1");
  auto t2 = bigtable::SetCell("f2", "c2", ms(0), "true2");
  auto f1 = bigtable::SetCell("f1", "c1", ms(0), "false1");
  auto f2 = bigtable::SetCell("f2", "c2", ms(0), "false2");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncCheckAndMutateRow)
      .WillOnce([this](CompletionQueue&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       auto, v2::CheckAndMutateRowRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            TransientError());
      })
      .WillOnce([this](CompletionQueue&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       auto, v2::CheckAndMutateRowRequest const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(*context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        return make_ready_future<StatusOr<v2::CheckAndMutateRowResponse>>(
            PermanentError());
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(1);
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptionsWithoutClientContextSetup()
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<IdempotentMutationPolicyOption>(
              bigtable::AlwaysRetryMutationPolicy().clone()));
  auto status = conn->AsyncCheckAndMutateRow(kTableName, "row", TestFilter(),
                                             {t1, t2}, {f1, f2});
  EXPECT_THAT(status.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, SampleRowsSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce([this](auto client_context, auto const&,
                       v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeSampleRowsResponse("test1", 11)))
            .WillOnce(Return(MakeSampleRowsResponse("test2", 22)))
            .WillOnce(Return(Status{}));
        return stream;
      });

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));
  auto samples = conn->SampleRows(kTableName);
  ASSERT_STATUS_OK(samples);
  auto actual = RowKeySampleVectors(*samples);
  EXPECT_THAT(actual.offset_bytes, ElementsAre(11, 22));
  EXPECT_THAT(actual.row_keys, ElementsAre("test1", "test2"));
}

TEST_F(DataConnectionTest, SampleRowsRetryResetsSamples) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce([](auto, auto const&, v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeSampleRowsResponse("discarded", 11)))
            .WillOnce(Return(TransientError()));
        return stream;
      })
      .WillOnce([](auto, auto const&, v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeSampleRowsResponse("returned", 22)))
            .WillOnce(Return(Status{}));
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto samples = conn->SampleRows(kTableName);
  ASSERT_STATUS_OK(samples);
  auto actual = RowKeySampleVectors(*samples);
  EXPECT_THAT(actual.offset_bytes, ElementsAre(22));
  EXPECT_THAT(actual.row_keys, ElementsAre("returned"));
}

TEST_F(DataConnectionTest, SampleRowsRetryExhausted) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](auto context, auto const&,
                             v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(TransientError()));
        return stream;
      });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(kNumRetries);
    return clone;
  });
  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions()
          .set<DataBackoffPolicyOption>(std::move(mock_b))
          .set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));
  auto samples = conn->SampleRows(kTableName);
  EXPECT_THAT(samples, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, SampleRowsPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce([this](auto client_context, auto const&,
                       v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
        return stream;
      });

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));
  auto samples = conn->SampleRows(kTableName);
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, SampleRowsBigtableCookie) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce(
          [this](auto context, auto const&, v2::SampleRowKeysRequest const&) {
            // Return a bigtable cookie in the first request.
            metadata_fixture_.SetServerMetadata(
                *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
            auto stream = std::make_unique<MockSampleRowKeysStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(TransientError()));
            return stream;
          })
      .WillOnce(
          [this](auto context, auto const&, v2::SampleRowKeysRequest const&) {
            // Verify that the next request includes the bigtable cookie from
            // above.
            auto headers = metadata_fixture_.GetMetadata(*context);
            EXPECT_THAT(headers,
                        Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
            auto stream = std::make_unique<MockSampleRowKeysStream>();
            EXPECT_CALL(*stream, Read).WillOnce(Return(PermanentError()));
            return stream;
          });

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(1);
    return clone;
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptionsWithoutClientContextSetup().set<DataBackoffPolicyOption>(
          std::move(mock_b)));
  auto samples = conn->SampleRows(kTableName);
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, SampleRowsRetryInfoHeeded) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce([](auto, auto const&, v2::SampleRowKeysRequest const&) {
        auto status = PermanentError();
        internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(status));
        return stream;
      })
      .WillOnce([](auto, auto const&, v2::SampleRowKeysRequest const&) {
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<EnableServerRetriesOption>(true));
  auto samples = conn->SampleRows(kTableName);
  EXPECT_STATUS_OK(samples);
}

TEST_F(DataConnectionTest, SampleRowsRetryInfoIgnored) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, SampleRowKeys)
      .WillOnce([](auto, auto const&, v2::SampleRowKeysRequest const&) {
        auto status = PermanentError();
        internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
        auto stream = std::make_unique<MockSampleRowKeysStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(status));
        return stream;
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions().set<EnableServerRetriesOption>(false));
  auto samples = conn->SampleRows(kTableName);
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

// The `AsyncRowSampler` is tested extensively in `async_row_sampler_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST_F(DataConnectionTest, AsyncSampleRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([](CompletionQueue const&, auto, auto,
                   v2::SampleRowKeysRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        using ErrorStream =
            internal::AsyncStreamingReadRpcError<v2::SampleRowKeysResponse>;
        return std::make_unique<ErrorStream>(PermanentError());
      });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions());
  auto samples = conn->AsyncSampleRows(kTableName).get();
  EXPECT_THAT(samples, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, ReadModifyWriteRowSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  v2::ReadModifyWriteRowResponse response;
  auto constexpr kText = R"pb(
    row {
      key: "row"
      families {
        name: "cf"
        columns {
          qualifier: "cq"
          cells { value: "value" }
        }
      }
    })pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([&response](grpc::ClientContext&, Options const&,
                            v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return response;
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto row = conn->ReadModifyWriteRow(req);
  ASSERT_STATUS_OK(row);
  EXPECT_EQ("row", row->row_key());

  auto c = bigtable::Cell("row", "cf", "cq", 0, "value");
  EXPECT_THAT(row->cells(), ElementsAre(MatchCell(c)));
}

TEST_F(DataConnectionTest, ReadModifyWriteRowPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return PermanentError();
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto row = conn->ReadModifyWriteRow(req);
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, ReadModifyWriteRowTransientErrorNotRetried) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadModifyWriteRow)
      .WillOnce([](grpc::ClientContext&, Options const&,
                   v2::ReadModifyWriteRowRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return TransientError();
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(0);
    return clone;
  });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto row = conn->ReadModifyWriteRow(req);
  EXPECT_THAT(row, StatusIs(StatusCode::kUnavailable));
}

TEST_F(DataConnectionTest, AsyncReadModifyWriteRowSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  v2::ReadModifyWriteRowResponse response;
  auto constexpr kText = R"pb(
    row {
      key: "row"
      families {
        name: "cf"
        columns {
          qualifier: "cq"
          cells { value: "value" }
        }
      }
    })pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &response));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([&response, this](
                    google::cloud::CompletionQueue&, auto client_context, auto,
                    v2::ReadModifyWriteRowRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future(make_status_or(response));
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));
  auto row = conn->AsyncReadModifyWriteRow(req).get();
  ASSERT_STATUS_OK(row);
  EXPECT_EQ("row", row->row_key());

  auto c = bigtable::Cell("row", "cf", "cq", 0, "value");
  EXPECT_THAT(row->cells(), ElementsAre(MatchCell(c)));
}

TEST_F(DataConnectionTest, AsyncReadModifyWriteRowPermanentError) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([this](google::cloud::CompletionQueue&, auto client_context,
                       auto, v2::ReadModifyWriteRowRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::ReadModifyWriteRowResponse>>(
            PermanentError());
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(
      CallOptions().set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));
  auto row = conn->AsyncReadModifyWriteRow(req).get();
  EXPECT_THAT(row, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(DataConnectionTest, AsyncReadModifyWriteRowTransientErrorNotRetried) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadModifyWriteRow)
      .WillOnce([this](google::cloud::CompletionQueue&, auto client_context,
                       auto, v2::ReadModifyWriteRowRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ("row", request.row_key());
        return make_ready_future<StatusOr<v2::ReadModifyWriteRowResponse>>(
            TransientError());
      });

  v2::ReadModifyWriteRowRequest req;
  req.set_app_profile_id(kAppProfile);
  req.set_table_name(kTableName);
  req.set_row_key("row");

  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([]() {
    auto clone = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone, OnCompletion).Times(0);
    return clone;
  });

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(
      CallOptions()
          .set<internal::GrpcSetupOption>(mock_setup.AsStdFunction())
          .set<DataBackoffPolicyOption>(std::move(mock_b)));
  auto row = conn->AsyncReadModifyWriteRow(req).get();
  EXPECT_THAT(row, StatusIs(StatusCode::kUnavailable));
}

// The `AsyncRowReader` is tested extensively in `async_row_reader_test.cc`.
// In this test, we just verify that the configuration is passed along.
TEST_F(DataConnectionTest, AsyncReadRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](CompletionQueue const&, auto, auto,
                   v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(42, request.rows_limit());
        EXPECT_THAT(request, HasTestRowSet());
        EXPECT_THAT(request.filter(), IsTestFilter());
        using ErrorStream =
            internal::AsyncStreamingReadRpcError<v2::ReadRowsResponse>;
        return std::make_unique<ErrorStream>(PermanentError());
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions());
  conn->AsyncReadRows(kTableName, on_row.AsStdFunction(),
                      on_finish.AsStdFunction(), TestRowSet(), 42,
                      TestFilter());
}

TEST_F(DataConnectionTest, AsyncReadRowsReverseScan) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](CompletionQueue const&, auto, auto,
                   v2::ReadRowsRequest const& request) {
        EXPECT_TRUE(request.reversed());
        using ErrorStream =
            internal::AsyncStreamingReadRpcError<v2::ReadRowsResponse>;
        return std::make_unique<ErrorStream>(PermanentError());
      });

  MockFunction<future<bool>(bigtable::Row const&)> on_row;
  EXPECT_CALL(on_row, Call).Times(0);

  MockFunction<void(Status)> on_finish;
  EXPECT_CALL(on_finish, Call).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
  });

  auto conn = TestConnection(std::move(mock));
  internal::OptionsSpan span(CallOptions().set<ReverseScanOption>(true));
  conn->AsyncReadRows(kTableName, on_row.AsStdFunction(),
                      on_finish.AsStdFunction(), TestRowSet(), 42,
                      TestFilter());
}

TEST_F(DataConnectionTest, AsyncReadRowEmpty) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](CompletionQueue const&, auto, auto,
                   v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future<absl::optional<v2::ReadRowsResponse>>({});
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto resp = conn->AsyncReadRow(kTableName, "row", TestFilter()).get();
  ASSERT_STATUS_OK(resp);
  EXPECT_FALSE(resp->first);
}

TEST_F(DataConnectionTest, AsyncReadRowSuccess) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(1);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](CompletionQueue const&, auto, auto,
                   v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        auto stream = std::make_unique<MockAsyncReadRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              v2::ReadRowsResponse r;
              auto& c = *r.add_chunks();
              c.set_row_key("row");
              c.mutable_family_name()->set_value("cf");
              c.mutable_qualifier()->set_value("cq");
              c.set_commit_row(true);
              return make_ready_future(absl::make_optional(r));
            })
            .WillOnce([] {
              return make_ready_future<absl::optional<v2::ReadRowsResponse>>(
                  {});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto resp = conn->AsyncReadRow(kTableName, "row", TestFilter()).get();
  ASSERT_STATUS_OK(resp);
  EXPECT_TRUE(resp->first);
  EXPECT_EQ("row", resp->second.row_key());
}

TEST_F(DataConnectionTest, AsyncReadRowFailure) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);
  EXPECT_CALL(*mock_metric, ElementRequest).Times(0);
  EXPECT_CALL(*mock_metric, ElementDelivery).Times(0);
  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  auto factory = std::make_unique<FakeOperationContextFactory>(
      ResourceLabels{}, DataLabels{}, fake_metric, clock);
#else
  auto factory = std::make_unique<SimpleOperationContextFactory>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncReadRows)
      .WillOnce([](CompletionQueue const&, auto, auto,
                   v2::ReadRowsRequest const& request) {
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_EQ(1, request.rows_limit());
        EXPECT_THAT(request.rows().row_keys(), ElementsAre("row"));
        EXPECT_THAT(request.filter(), IsTestFilter());

        using ErrorStream =
            internal::AsyncStreamingReadRpcError<v2::ReadRowsResponse>;
        return std::make_unique<ErrorStream>(PermanentError());
      });

  auto conn = TestConnection(std::move(mock), std::move(factory));
  internal::OptionsSpan span(CallOptions());
  auto resp = conn->AsyncReadRow(kTableName, "row", TestFilter()).get();
  EXPECT_THAT(resp, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
