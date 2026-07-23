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
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {
namespace {

bool IsDirectPathReachable() {
  int s = socket(AF_INET6, SOCK_STREAM, 0);
  if (s < 0) return false;
  sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(443);
  inet_pton(AF_INET6, "2607:f8b0:4001:c2f::5f", &addr.sin6_addr);
  timeval tv{1, 0};
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  int res = connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  close(s);
  return res == 0;
}

using ::google::cloud::bigtable::testing::TableTestEnvironment;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::StartsWith;

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

void VerifyResourceLabels(google::monitoring::v3::TimeSeries const& ts,
                          std::string const& expected_project_id,
                          std::string const& expected_instance_id,
                          std::string const& expected_table_id) {
  auto const& labels = ts.resource().labels();
  auto project_it = labels.find("project_id");
  if (project_it != labels.end()) {
    EXPECT_EQ(project_it->second, expected_project_id);
  }
  auto instance_it = labels.find("instance");
  if (instance_it != labels.end()) {
    EXPECT_EQ(instance_it->second, expected_instance_id);
  }
  auto table_it = labels.find("table");
  if (table_it != labels.end()) {
    EXPECT_EQ(table_it->second, expected_table_id);
  }
  auto zone_it = labels.find("zone");
  if (zone_it != labels.end() && !TableTestEnvironment::zone_a().empty()) {
    std::vector<absl::string_view> parts =
        absl::StrSplit(TableTestEnvironment::zone_a(), '-');
    auto prefix = parts.size() >= 2 ? absl::StrCat(parts[0], "-", parts[1])
                                    : TableTestEnvironment::zone_a();
    EXPECT_THAT(zone_it->second, StartsWith(prefix));
  }
}

TEST_F(ObservabilityIntegrationTest, VerifyOperationAndAttemptMetrics) {
  if (UsingCloudBigtableEmulator()) {
    GTEST_SKIP() << "Metrics export integration test runs against production";
  }

  // Set MetricsPeriodOption to 5s (minimum allowed by DefaultOptions; smaller
  // periods reset to 60s)
  auto options = Options{}
                     .set<EnableMetricsOption>(true)
                     .set<MetricsPeriodOption>(std::chrono::seconds(5))
                     .set<MinConnectionRefreshOption>(std::chrono::hours(1))
                     .set<MaxConnectionRefreshOption>(std::chrono::hours(1));

  auto const table_id = TableTestEnvironment::table_id();

  // Add scoped connection to ensure metrics are flushed on destruction.
  {
    auto conn = MakeDataConnection(
        {InstanceResource(Project(project_id()), instance_id())}, options);
    auto table = Table(std::move(conn),
                       TableResource(project_id(), instance_id(), table_id));

    std::string const row_key = "observability-row-1";
    std::vector<Cell> expected{{row_key, "family4", "c0", 1000, "v1000"},
                               {row_key, "family4", "c1", 2000, "v2000"}};

    // Perform mutations and read calls
    Apply(table, row_key, expected);
    auto actual = ReadRows(table, Filter::PassAllFilter());
    CheckEqualUnordered(expected, actual);

    // Wait for the periodic 5-second exporter background thread to flush
    // metrics while conn is active
    std::this_thread::sleep_for(std::chrono::seconds(6));
  }

  auto recorded = collector_service_.recorded_metrics();
  ASSERT_FALSE(recorded.empty());

  bool found_operation_latencies = false;
  bool found_attempt_latencies = false;

  for (auto const& req : recorded) {
    EXPECT_EQ(req.name(), absl::StrCat("projects/", project_id()));

    for (auto const& ts : req.time_series()) {
      auto const& metric_type = ts.metric().type();
      // Skip any non-bigtable metrics (e.g. gRPC metrics) that might be
      // exported since we globally enabled them in the client connection.
      // This test only validates custom Bigtable client metrics.
      if (!absl::StartsWith(metric_type,
                            "bigtable.googleapis.com/internal/client/")) {
        continue;
      }

      if (absl::StrContains(metric_type, "operation_latencies")) {
        found_operation_latencies = true;
      }
      if (absl::StrContains(metric_type, "attempt_latencies")) {
        found_attempt_latencies = true;
      }

      VerifyResourceLabels(ts, project_id(), instance_id(), table_id);
    }
  }

  EXPECT_TRUE(found_operation_latencies);
  EXPECT_TRUE(found_attempt_latencies);
}

bool VerifyDirectPathGrpcResourceLabels(
    google::monitoring::v3::TimeSeries const& ts,
    absl::optional<std::string> const& expected_location,
    absl::optional<std::string> const& expected_cloud_platform,
    std::string const& expected_client_project,
    absl::optional<std::string> const& expected_hostname) {
  auto const& labels = ts.resource().labels();
  auto location_it = labels.find("location");
  auto platform_it = labels.find("cloud_platform");
  auto host_id_it = labels.find("host_id");
  auto client_project_it = labels.find("client_project");

  if (location_it == labels.end() || platform_it == labels.end() ||
      host_id_it == labels.end() || client_project_it == labels.end()) {
    return false;
  }

  EXPECT_FALSE(location_it->second.empty());
  if (expected_location.has_value() && !expected_location->empty()) {
    std::vector<absl::string_view> parts =
        absl::StrSplit(*expected_location, '-');
    auto region_prefix = parts.size() >= 2
                             ? absl::StrCat(parts[0], "-", parts[1])
                             : *expected_location;
    EXPECT_THAT(location_it->second, StartsWith(region_prefix));
  }

  EXPECT_FALSE(platform_it->second.empty());
  if (expected_cloud_platform.has_value() &&
      !expected_cloud_platform->empty()) {
    EXPECT_EQ(platform_it->second, *expected_cloud_platform);
  }

  EXPECT_FALSE(host_id_it->second.empty());

  EXPECT_FALSE(client_project_it->second.empty());
  if (!expected_client_project.empty()) {
    EXPECT_EQ(client_project_it->second, expected_client_project);
  }

  if (expected_hostname.has_value() && !expected_hostname->empty()) {
    auto hostname_it = labels.find("hostname");
    if (hostname_it != labels.end()) {
      EXPECT_EQ(hostname_it->second, *expected_hostname);
    }
  }

  return true;
}

std::set<std::string> ProcessRecordedGrpcMetrics(
    std::vector<google::monitoring::v3::CreateTimeSeriesRequest> const&
        recorded,
    std::string const& project_id,
    absl::optional<std::string> const& expected_location,
    absl::optional<std::string> const& expected_cloud_platform,
    std::string const& expected_client_project,
    absl::optional<std::string> const& expected_hostname,
    bool& verified_resource_labels) {
  std::set<std::string> grpc_metric_types;
  for (auto const& req : recorded) {
    std::cout << "req=" << req.DebugString() << std::endl;
    EXPECT_EQ(req.name(), absl::StrCat("projects/", project_id));

    for (auto const& ts : req.time_series()) {
      auto const& metric_type = ts.metric().type();
      if (absl::StartsWith(metric_type, "workload.googleapis.com/grpc.")) {
        grpc_metric_types.insert(metric_type);
        if (VerifyDirectPathGrpcResourceLabels(
                ts, expected_location, expected_cloud_platform,
                expected_client_project, expected_hostname)) {
          verified_resource_labels = true;
        }
      }
    }
  }
  return grpc_metric_types;
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

  ScopedEnvironment env_dp("CBT_ENABLE_DIRECTPATH", "true");

  // Set MetricsPeriodOption to 5s (minimum allowed by DefaultOptions; smaller
  // periods reset to 60s)
  auto options = Options{}
                     .set<EnableMetricsOption>(true)
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
  ASSERT_FALSE(recorded.empty());

  bool verified_resource_labels = false;

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

  auto grpc_metric_types = ProcessRecordedGrpcMetrics(
      recorded, project_id(), expected_location, expected_cloud_platform,
      expected_client_project, expected_hostname, verified_resource_labels);

  EXPECT_TRUE(verified_resource_labels);

  // Verify that specific gRPC client metrics configured in GrpcMetricsExporter
  // are present. OpenTelemetry metric names are exported to Cloud Monitoring
  // with the "workload.googleapis.com/" prefix.
  //
  // Note: Event-driven and failure-driven metrics configured in
  // GrpcMetricsExporter (such as grpc.lb.rls.*, grpc.xds_client.*, and
  // grpc.subchannel.* disconnections/failures) are only exported when those
  // specific events or errors occur during the export window. Therefore, only
  // RPC attempt metrics (duration, started, message sizes) are guaranteed to
  // produce time series during a healthy test run.
  EXPECT_THAT(grpc_metric_types,
              ::testing::Contains(
                  "workload.googleapis.com/grpc.client.attempt.duration"));
  EXPECT_THAT(grpc_metric_types,
              ::testing::Contains(
                  "workload.googleapis.com/grpc.client.attempt.started"));
}

}  // namespace
}  // namespace testing
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
