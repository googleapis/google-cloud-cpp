// Copyright 2026 Google LLC
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

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "ci/otel_collector/otel_collector.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::TableTestEnvironment;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;
using ::testing::StartsWith;

MATCHER_P(MetricType, matcher, "") {
  return ExplainMatchResult(matcher, arg.metric().type(), result_listener);
}

MATCHER_P(ResourceType, matcher, "") {
  return ExplainMatchResult(matcher, arg.resource().type(), result_listener);
}

MATCHER_P2(HasMetricLabel, key, val_matcher, "") {
  auto const& labels = arg.metric().labels();
  auto it = labels.find(key);
  if (it == labels.end()) {
    *result_listener << "no metric label '" << key << "'";
    return false;
  }
  return ExplainMatchResult(val_matcher, it->second, result_listener);
}

MATCHER_P2(HasResourceLabel, key, val_matcher, "") {
  auto const& labels = arg.resource().labels();
  auto it = labels.find(key);
  if (it == labels.end()) {
    *result_listener << "no resource label '" << key << "'";
    return false;
  }
  return ExplainMatchResult(val_matcher, it->second, result_listener);
}

MATCHER_P(HasTimeSeries, ts_matcher, "") {
  return ExplainMatchResult(Contains(ts_matcher), arg.time_series(),
                            result_listener);
}

class ObservabilityIntegrationTest
    : public ::google::cloud::bigtable::testing::TableIntegrationTest {
 protected:
  void SetUp() override {
    TableIntegrationTest::SetUp();
    int port = 0;
    grpc::ServerBuilder builder;
    builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                             &port);
    builder.RegisterService(
        static_cast<google::monitoring::v3::MetricService::Service*>(
            &collector_service_));
    builder.RegisterService(
        static_cast<::google::cloud::opentelemetry::testing::
                        ObservabilityVerificationService::Service*>(
            &collector_service_));
    server_ = builder.BuildAndStart();
    server_address_ = absl::StrCat("localhost:", port);
  }

  void TearDown() override {
    if (server_) {
      server_->Shutdown();
      server_->Wait();
    }
    TableIntegrationTest::TearDown();
  }

  google::cloud::testing_util::OtelCollectorServer collector_service_;
  std::unique_ptr<grpc::Server> server_;
  std::string server_address_;
};

/// Use Table::Apply() to insert a single row.
void Apply(Table& table, std::string const& row_key,
           std::vector<Cell> const& cells) {
  auto mutation = SingleRowMutation(row_key);
  for (auto const& cell : cells) {
    mutation.emplace_back(
        SetCell(cell.family_name(), cell.column_qualifier(),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::microseconds(cell.timestamp())),
                cell.value()));
  }
  auto status = table.Apply(std::move(mutation));
  ASSERT_STATUS_OK(status);
}

TEST_F(ObservabilityIntegrationTest, VerifyOperationAndAttemptMetrics) {
  if (UsingCloudBigtableEmulator()) {
    GTEST_SKIP() << "Metrics export integration test runs against production";
  }

  // Redirect Cloud Monitoring metric export to local otel_collector
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_METRIC_SERVICE_ENDPOINT",
                        server_address_);
  ScopedEnvironment env_otel("GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR", "1");

  // Set MetricsPeriodOption to 5s (minimum allowed by DefaultOptions; smaller
  // periods reset to 60s)
  auto options =
      Options{}.set<EnableMetricsOption>(true).set<MetricsPeriodOption>(
          std::chrono::seconds(5));

  auto const table_id = TableTestEnvironment::table_id();
  bool const is_dynamic = google::cloud::internal::GetEnv(
                              "GOOGLE_CLOUD_CPP_BIGTABLE_TESTING_CHANNEL_POOL")
                              .value_or("") == "dynamic";

  // Add scoped connection to ensure metrics are flushed on destruction.
  {
    std::shared_ptr<DataConnection> conn;
    if (is_dynamic) {
      conn = MakeDataConnection(
          {InstanceResource(Project(project_id()), instance_id())}, options);
    } else {
      conn = MakeDataConnection(options);
    }
    auto table = Table(std::move(conn),
                       TableResource(project_id(), instance_id(), table_id));

    std::string const row_key = "observability-row-1";
    std::vector<Cell> expected{{row_key, "family4", "c0", 1000, "v1000"},
                               {row_key, "family4", "c1", 2000, "v2000"}};

    // Perform mutations and read calls
    Apply(table, row_key, expected);
    auto actual = ReadRows(table, Filter::RowKeysRegex(row_key));
    CheckEqualUnordered(expected, actual);

    // Wait for the periodic 5-second exporter background thread to flush
    // metrics while conn is active
    std::this_thread::sleep_for(std::chrono::seconds(6));
  }

  auto recorded = collector_service_.recorded_metrics();
  ASSERT_THAT(recorded, Not(IsEmpty()));
  EXPECT_THAT(
      recorded,
      Each(Property(&google::monitoring::v3::CreateTimeSeriesRequest::name,
                    Eq(absl::StrCat("projects/", project_id())))));

  auto has_resource_labels = AllOf(HasResourceLabel("project_id", project_id()),
                                   HasResourceLabel("instance", instance_id()),
                                   HasResourceLabel("table", table_id));

  if (!TableTestEnvironment::zone_a().empty()) {
    std::vector<absl::string_view> parts =
        absl::StrSplit(TableTestEnvironment::zone_a(), '-');
    auto prefix = parts.size() >= 2 ? absl::StrCat(parts[0], "-", parts[1])
                                    : TableTestEnvironment::zone_a();
    EXPECT_THAT(recorded, Contains(HasTimeSeries(AllOf(
                              MetricType(HasSubstr("operation_latencies")),
                              has_resource_labels,
                              HasResourceLabel("zone", StartsWith(prefix))))));
    EXPECT_THAT(recorded, Contains(HasTimeSeries(AllOf(
                              MetricType(HasSubstr("attempt_latencies")),
                              has_resource_labels,
                              HasResourceLabel("zone", StartsWith(prefix))))));
  } else {
    EXPECT_THAT(recorded, Contains(HasTimeSeries(AllOf(
                              MetricType(HasSubstr("operation_latencies")),
                              has_resource_labels))));
    EXPECT_THAT(
        recorded,
        Contains(HasTimeSeries(AllOf(MetricType(HasSubstr("attempt_latencies")),
                                     has_resource_labels))));
  }
}

TEST_F(ObservabilityIntegrationTest, VerifyOutstandingRpcsMetric) {
  if (UsingCloudBigtableEmulator()) {
    GTEST_SKIP() << "Metrics export integration test runs against production";
  }

  // Redirect Cloud Monitoring metric export to local otel_collector
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_METRIC_SERVICE_ENDPOINT",
                        server_address_);
  ScopedEnvironment env_otel("GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR", "1");

  // Set MetricsPeriodOption to 5s (minimum allowed by DefaultOptions; smaller
  // periods reset to 60s)
  auto options =
      Options{}.set<EnableMetricsOption>(true).set<MetricsPeriodOption>(
          std::chrono::seconds(5));

  auto const table_id = TableTestEnvironment::table_id();
  bool const is_dynamic = google::cloud::internal::GetEnv(
                              "GOOGLE_CLOUD_CPP_BIGTABLE_TESTING_CHANNEL_POOL")
                              .value_or("") == "dynamic";
  std::string const expected_lb_policy =
      is_dynamic ? "RANDOM_TWO_LEAST_USED" : "ROUND_ROBIN";

  collector_service_.Clear();
  {
    std::shared_ptr<DataConnection> conn;
    if (is_dynamic) {
      conn = MakeDataConnection(
          {InstanceResource(Project(project_id()), instance_id())}, options);
    } else {
      conn = MakeDataConnection(options);
    }
    auto table = Table(std::move(conn),
                       TableResource(project_id(), instance_id(), table_id));

    std::string const row_key = "observability-rpc-" + expected_lb_policy;
    std::vector<Cell> expected{{row_key, "family4", "c0", 1000, "v1000"},
                               {row_key, "family4", "c1", 2000, "v2000"}};

    // Perform mutations and read calls
    Apply(table, row_key, expected);
    auto actual = ReadRows(table, Filter::RowKeysRegex(row_key));
    CheckEqualUnordered(expected, actual);

    // Wait for the periodic 5-second exporter background thread to flush
    // metrics while conn is active
    std::this_thread::sleep_for(std::chrono::seconds(6));
  }

  auto recorded = collector_service_.recorded_metrics();
  ASSERT_THAT(recorded, Not(IsEmpty()));
  EXPECT_THAT(
      recorded,
      Each(Property(&google::monitoring::v3::CreateTimeSeriesRequest::name,
                    Eq(absl::StrCat("projects/", project_id())))));

  EXPECT_THAT(recorded,
              Contains(HasTimeSeries(AllOf(
                  MetricType(HasSubstr("connection_pool/outstanding_rpcs")),
                  ResourceType("bigtable.googleapis.com/Client"),
                  HasResourceLabel("project_id", project_id()),
                  HasResourceLabel("instance", instance_id()),
                  HasMetricLabel("channel_pool_lb_policy", expected_lb_policy),
                  HasMetricLabel("transport_type", Not(IsEmpty())),
                  HasMetricLabel("streaming", Not(IsEmpty()))))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new ::google::cloud::bigtable::testing::TableTestEnvironment);
  return RUN_ALL_TESTS();
}
