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

#ifndef _WIN32

#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "ci/otel_collector/otel_collector.h"
#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <netinet/in.h>
#include <cerrno>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

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
using ::testing::Matcher;
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

bool IsDirectPathReachable() {
  int s = socket(AF_INET6, SOCK_STREAM, 0);
  if (s < 0) return false;
  int flags = fcntl(s, F_GETFL, 0);
  (void)fcntl(s, F_SETFL, flags | O_NONBLOCK);
  sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(443);
  inet_pton(AF_INET6, "2607:f8b0:4001:c2f::5f", &addr.sin6_addr);
  int res = connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  if (res < 0 && errno == EINPROGRESS) {
    pollfd pfd{s, POLLOUT, 0};
    res = poll(&pfd, 1, 1000);
    if (res > 0) {
      int err = 0;
      socklen_t len = sizeof(err);
      (void)getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
      res = (err == 0) ? 0 : -1;
    } else {
      res = -1;
    }
  }
  close(s);
  return res == 0;
}

class ObservabilityIntegrationTest
    : public ::google::cloud::bigtable::testing::TableIntegrationTest {
 protected:
  static void SetUpTestSuite() {
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
    env_endpoint_ = std::make_unique<ScopedEnvironment>(
        "GOOGLE_CLOUD_CPP_METRIC_SERVICE_ENDPOINT", server_address_);
    env_otel_ = std::make_unique<ScopedEnvironment>(
        "GOOGLE_CLOUD_CPP_TESTING_OTEL_COLLECTOR", "1");
  }

  static void TearDownTestSuite() {
    env_endpoint_.reset();
    env_otel_.reset();
    if (server_) {
      server_->Shutdown(std::chrono::system_clock::now() +
                        std::chrono::seconds(1));
      server_->Wait();
    }
  }

  void SetUp() override {
    TableIntegrationTest::SetUp();
    data_connection_.reset();
    collector_service_.Clear();
  }

  void TearDown() override { TableIntegrationTest::TearDown(); }

  static google::cloud::testing_util::OtelCollectorServer collector_service_;
  static std::unique_ptr<grpc::Server> server_;
  static std::string server_address_;
  static std::unique_ptr<ScopedEnvironment> env_endpoint_;
  static std::unique_ptr<ScopedEnvironment> env_otel_;
};

google::cloud::testing_util::OtelCollectorServer
    ObservabilityIntegrationTest::collector_service_;
std::unique_ptr<grpc::Server> ObservabilityIntegrationTest::server_;
std::string ObservabilityIntegrationTest::server_address_;
std::unique_ptr<ScopedEnvironment> ObservabilityIntegrationTest::env_endpoint_;
std::unique_ptr<ScopedEnvironment> ObservabilityIntegrationTest::env_otel_;

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
  auto options = Options{}
                     .set<EnableMetricsOption>(true)
                     .set<MetricsPeriodOption>(std::chrono::seconds(5))
                     .set<MinConnectionRefreshOption>(std::chrono::hours(1))
                     .set<MaxConnectionRefreshOption>(std::chrono::hours(1));

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

TEST_F(ObservabilityIntegrationTest, VerifyDirectPathGrpcMetrics) {
  if (UsingCloudBigtableEmulator()) {
    GTEST_SKIP() << "Metrics export integration test runs against production";
  }

  auto disable_direct_path =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_DISABLE_DIRECT_PATH")
          .value_or("");
  if (disable_direct_path == "true" || !IsDirectPathReachable()) {
    GTEST_SKIP() << "DirectPath is disabled or network is unreachable in this "
                    "test environment";
  }

  // Set MetricsPeriodOption to 5s (minimum allowed by DefaultOptions; smaller
  // periods reset to 60s)
  auto options = Options{}
                     .set<EnableMetricsOption>(true)
                     .set<experimental::DirectPathModeOption>(
                         experimental::DirectPathMode::kEnabled)
                     .set<MetricsPeriodOption>(std::chrono::seconds(5))
                     .set<MinConnectionRefreshOption>(std::chrono::hours(1))
                     .set<MaxConnectionRefreshOption>(std::chrono::hours(1))
                     .set<GrpcChannelArgumentsOption>({
                         {"grpc.client_idle_timeout_ms", "1000"},
                         {"grpc.max_reconnect_backoff_ms", "1000"},
                     });

  auto const& table_id = TableTestEnvironment::table_id();

  // Add scoped connection to ensure metrics are flushed on destruction.
  {
    auto conn = MakeDataConnection(
        {InstanceResource(Project(project_id()), instance_id())}, options);
    auto table = Table(std::move(conn),
                       TableResource(project_id(), instance_id(), table_id));

    std::string const row_key = "observability-directpath-row-1";
    std::vector<Cell> expected{
        {row_key, "family4", "c0", 1000, "v1000"},
        {row_key, "family4", "c1", 2000, "v2000"},
    };

    // Perform mutations and read calls over DirectPath
    Apply(table, row_key, expected);
    auto actual = ReadRows(table, Filter::PassAllFilter());
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

  auto expected_client_project =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_TEST_EXPECTED_CLIENT_PROJECT")
          .value_or(project_id());
  auto expected_location = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_TEST_EXPECTED_LOCATION");
  auto expected_cloud_platform = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_TEST_EXPECTED_CLOUD_PLATFORM");
  auto expected_hostname = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_TEST_EXPECTED_HOSTNAME");

  Matcher<std::string const&> region_val_matcher = Not(IsEmpty());
  if (expected_location.has_value() && !expected_location->empty()) {
    std::vector<absl::string_view> parts =
        absl::StrSplit(*expected_location, '-');
    std::string region_prefix = parts.size() >= 2
                                    ? absl::StrCat(parts[0], "-", parts[1])
                                    : *expected_location;
    region_val_matcher = StartsWith(region_prefix);
  }

  Matcher<std::string const&> platform_val_matcher = Not(IsEmpty());
  if (expected_cloud_platform.has_value() &&
      !expected_cloud_platform->empty()) {
    platform_val_matcher = Eq(*expected_cloud_platform);
  }

  auto directpath_resource_labels =
      AllOf(ResourceType("bigtable_client"),
            HasResourceLabel("region", region_val_matcher),
            HasResourceLabel("cloud_platform", platform_val_matcher),
            HasResourceLabel("client_project", Eq(expected_client_project)),
            HasResourceLabel("host_id", Not(IsEmpty())));

  // Verify that specific gRPC client metrics configured in GrpcMetricsExporter
  // are present with the expected DirectPath bigtable_client MonitoredResource
  // labels.
  //
  // Note: Event-driven and failure-driven metrics configured in
  // GrpcMetricsExporter (such as grpc.lb.rls.*, grpc.xds_client.*, and
  // grpc.subchannel.* disconnections/failures) are only exported when those
  // specific events or errors occur during the export window. Therefore, only
  // RPC attempt duration metric is guaranteed to produce time series during a
  // healthy test run.
  if (expected_hostname.has_value() && !expected_hostname->empty()) {
    EXPECT_THAT(recorded,
                Contains(HasTimeSeries(AllOf(
                    MetricType(HasSubstr("grpc/client/attempt/duration")),
                    directpath_resource_labels,
                    HasResourceLabel("host_name", Eq(*expected_hostname))))));
  } else {
    EXPECT_THAT(recorded,
                Contains(HasTimeSeries(
                    AllOf(MetricType(HasSubstr("grpc/client/attempt/duration")),
                          directpath_resource_labels))));
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

  std::string const table_id = TableTestEnvironment::table_id();
  bool const is_dynamic = google::cloud::internal::GetEnv(
                              "GOOGLE_CLOUD_CPP_BIGTABLE_TESTING_CHANNEL_POOL")
                              .value_or("") == "dynamic";
  if (!is_dynamic) {
    GTEST_SKIP()
        << "OutstandingRpcs metric is only supported for dynamic channel pools";
  }

  collector_service_.Clear();
  {
    std::shared_ptr<DataConnection> conn = MakeDataConnection(
        {InstanceResource(Project(project_id()), instance_id())}, options);
    Table table(std::move(conn),
                TableResource(project_id(), instance_id(), table_id));

    std::string const row_key = "observability-rpc-RANDOM_TWO_LEAST_USED";
    std::vector<Cell> expected{{row_key, "family4", "c0", 1000, "v1000"},
                               {row_key, "family4", "c1", 2000, "v2000"}};

    // Perform mutations and read calls
    Apply(table, row_key, expected);
    std::vector<Cell> actual = ReadRows(table, Filter::RowKeysRegex(row_key));
    CheckEqualUnordered(expected, actual);

    // Wait for the periodic 5-second exporter background thread to flush
    // metrics while conn is active
    std::this_thread::sleep_for(std::chrono::seconds(6));
  }

  std::vector<google::monitoring::v3::CreateTimeSeriesRequest> recorded =
      collector_service_.recorded_metrics();
  ASSERT_THAT(recorded, Not(IsEmpty()));
  EXPECT_THAT(
      recorded,
      Each(Property(&google::monitoring::v3::CreateTimeSeriesRequest::name,
                    Eq(absl::StrCat("projects/", project_id())))));

  EXPECT_THAT(
      recorded,
      Contains(HasTimeSeries(AllOf(
          MetricType(HasSubstr("connection_pool/outstanding_rpcs")),
          ResourceType("bigtable_client_raw"),
          HasResourceLabel("project_id", project_id()),
          HasResourceLabel("instance", instance_id()),
          HasMetricLabel("channel_pool_lb_policy", "RANDOM_TWO_LEAST_USED"),
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

#endif  // _WIN32
