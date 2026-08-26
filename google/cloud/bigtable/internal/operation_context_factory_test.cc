// Copyright 2025 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/operation_context_factory.h"
#include "google/cloud/bigtable/internal/client_schema_metrics.h"
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/internal/table_schema_metrics.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/monitoring/v3/metric_connection.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::SizeIs;

class MockMetric : public TableSchemaMetric {
 public:
  MOCK_METHOD(std::unique_ptr<TableSchemaMetric>, clone,
              (TableResourceLabels const&, TableDataLabels const&),
              (const, override));
};

TEST(MetricsOperationContextFactoryTest, ReadRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](TableResourceLabels const& resource_labels,
                    TableDataLabels const& data_labels) {
        EXPECT_THAT(resource_labels.project_id, Eq("my-project"));
        EXPECT_THAT(resource_labels.instance, Eq("my-instance"));
        EXPECT_THAT(resource_labels.table, Eq("my-table"));
        EXPECT_THAT(resource_labels.cluster, IsEmpty());
        EXPECT_THAT(resource_labels.zone, IsEmpty());
        EXPECT_THAT(data_labels.method, Eq("ReadRow"));
        EXPECT_THAT(data_labels.streaming, Eq("true"));
        EXPECT_THAT(data_labels.client_name,
                    Eq("cpp.Bigtable/" + version_string()));
        EXPECT_THAT(data_labels.client_uid, Eq("my-client-uid"));
        EXPECT_THAT(data_labels.app_profile, Eq(app_profile));
        EXPECT_THAT(data_labels.status, IsEmpty());
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory("my-client-uid", mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.ReadRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, ReadRows) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("ReadRows"));
            EXPECT_THAT(data_labels.streaming, Eq("true"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.ReadRows(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, MutateRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("MutateRow"));
            EXPECT_THAT(data_labels.streaming, Eq("false"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.MutateRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, MutateRows) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("MutateRows"));
            EXPECT_THAT(data_labels.streaming, Eq("true"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.MutateRows(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, CheckAndMutateRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("CheckAndMutateRow"));
            EXPECT_THAT(data_labels.streaming, Eq("false"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.CheckAndMutateRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, SampleRowKeys) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("SampleRowKeys"));
            EXPECT_THAT(data_labels.streaming, Eq("true"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.SampleRowKeys(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, ReadModifyWriteRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("ReadModifyWriteRow"));
            EXPECT_THAT(data_labels.streaming, Eq("false"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.ReadModifyWriteRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, PrepareQuery) {
  std::string app_profile = "my-app-profile";
  std::string instance_full_name = "projects/my-project/instances/my-instance";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("PrepareQuery"));
            EXPECT_THAT(data_labels.streaming, Eq("false"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.PrepareQuery(instance_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, ExecuteQuery) {
  std::string app_profile = "my-app-profile";
  std::string instance_full_name = "projects/my-project/instances/my-instance";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce(
          [&](TableResourceLabels const&, TableDataLabels const& data_labels) {
            EXPECT_THAT(data_labels.method, Eq("ExecuteQuery"));
            EXPECT_THAT(data_labels.streaming, Eq("true"));
            return std::make_unique<MockMetric>();
          });

  MetricsOperationContextFactory factory({}, mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.ExecuteQuery(instance_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, IncludesOutstandingRpcs) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";
  auto options =
      Options{}.set<bigtable::MetricsPeriodOption>(std::chrono::seconds(60));
  MetricsOperationContextFactory factory(
      "test-uid",
      std::shared_ptr<monitoring_v3::MetricServiceConnection>(nullptr),
      std::move(options));
  std::shared_ptr<OperationContext> operation_context =
      factory.ReadRow(table_full_name, app_profile);
  EXPECT_THAT(operation_context, NotNull());
  operation_context->StubSelection(
      StubSelectionParams{10, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
                          TransportType::kDirectPath, RpcType::kUnary});
}

class MockClientMetric : public ClientSchemaMetric {
 public:
  MOCK_METHOD(std::unique_ptr<ClientSchemaMetric>, clone,
              (ClientResourceLabels const&), (const, override));
};

TEST(MetricsOperationContextFactoryTest, ClientResourceLabelsPopulated) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockClientMetric const>();
  EXPECT_CALL(*mock_metric, clone(::testing::A<ClientResourceLabels const&>()))
      .WillOnce([&](ClientResourceLabels const& client_labels) {
        EXPECT_THAT(client_labels.project_id, Eq("my-project"));
        EXPECT_THAT(client_labels.instance, Eq("my-instance"));
        EXPECT_THAT(client_labels.app_profile, Eq("my-app-profile"));
        EXPECT_THAT(client_labels.client_name,
                    Eq("cpp.Bigtable/" + version_string()));
        EXPECT_THAT(client_labels.client_uid, Eq("my-client-uid"));
        return std::make_unique<MockClientMetric>();
      });

  MetricsOperationContextFactory factory("my-client-uid", mock_metric);
  std::shared_ptr<OperationContext> operation_context =
      factory.ReadRow(table_full_name, app_profile);
  EXPECT_THAT(operation_context, NotNull());
}

class FakeTableMetric : public TableSchemaMetric {
 public:
  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const&, TableDataLabels const&) const override {
    return std::make_unique<FakeTableMetric>(*this);
  }
};

class FakeClientMetric : public ClientSchemaMetric {
 public:
  std::unique_ptr<ClientSchemaMetric> clone(
      ClientResourceLabels const&) const override {
    return std::make_unique<FakeClientMetric>(*this);
  }
};

TEST(MetricsOperationContextFactoryTest, CloneMetrics) {
  auto table_metric = std::make_shared<FakeTableMetric>();
  auto client_metric = std::make_shared<FakeClientMetric>();

  TableResourceLabels resource_labels{"project", "instance", "table", "cluster",
                                      "zone"};
  TableDataLabels data_labels{"method", "streaming", "client",
                              "uid",    "profile",   "status"};

  std::vector<std::shared_ptr<Metric const>> metrics = {table_metric,
                                                        client_metric};
  std::vector<std::shared_ptr<Metric>> cloned =
      CloneMetrics(resource_labels, data_labels, metrics);
  EXPECT_THAT(cloned, SizeIs(2));
}

TEST(MetricsOperationContextFactoryTest, CloneMetricsWithClientResourceLabels) {
  auto table_metric = std::make_shared<FakeTableMetric>();
  auto client_metric = std::make_shared<FakeClientMetric>();

  TableResourceLabels resource_labels{"project", "instance", "table", "cluster",
                                      "zone"};
  TableDataLabels data_labels{"method", "streaming", "client",
                              "uid",    "profile",   "status"};
  ClientResourceLabels client_labels{
      "project",     "instance", "profile", "client", "uid",
      "client-proj", "region",   "gcp",     "host",   "hostname"};

  std::vector<std::shared_ptr<Metric const>> metrics = {table_metric,
                                                        client_metric};
  std::vector<std::shared_ptr<Metric>> cloned =
      CloneMetrics(resource_labels, data_labels, client_labels, metrics);
  EXPECT_THAT(cloned, SizeIs(2));
}

class MockMetricServiceConnection
    : public monitoring_v3::MetricServiceConnection {
 public:
  ~MockMetricServiceConnection() override = default;
  MOCK_METHOD(Status, CreateServiceTimeSeries,
              (google::monitoring::v3::CreateTimeSeriesRequest const&),
              (override));
  MOCK_METHOD(Status, CreateTimeSeries,
              (google::monitoring::v3::CreateTimeSeriesRequest const&),
              (override));
};

TEST(MetricsOperationContextFactoryTest,
     InitializeProviderExportsTableAndClientMetrics) {
  auto mock_conn = std::make_shared<MockMetricServiceConnection>();

  EXPECT_CALL(*mock_conn, CreateServiceTimeSeries)
      .WillRepeatedly(
          [](google::monitoring::v3::CreateTimeSeriesRequest const& request) {
            EXPECT_THAT(request.name(), Eq("projects/my-project"));
            for (auto const& ts : request.time_series()) {
              if (ts.resource().labels().find("table") !=
                  ts.resource().labels().end()) {
                EXPECT_THAT(ts.resource().type(), Eq("bigtable_client_raw"));
                EXPECT_THAT(ts.resource().labels().at("project_id"),
                            Eq("my-project"));
                EXPECT_THAT(ts.resource().labels().at("instance"),
                            Eq("my-instance"));
                EXPECT_THAT(ts.resource().labels().at("table"), Eq("my-table"));
              } else {
                EXPECT_THAT(ts.resource().type(), Eq("bigtable_client"));
                EXPECT_THAT(ts.resource().labels().at("project_id"),
                            Eq("my-project"));
                EXPECT_THAT(ts.resource().labels().at("instance"),
                            Eq("my-instance"));
                EXPECT_THAT(ts.resource().labels().at("app_profile"),
                            Eq("my-app-profile"));
                EXPECT_THAT(ts.resource().labels().at("uuid"), Eq("test-uid"));
                EXPECT_THAT(ts.resource().labels().at("client_name"),
                            Eq("cpp.Bigtable/" + version_string()));
              }
              // Verify resource filtering removed resource labels from metric
              // labels:
              EXPECT_TRUE(ts.metric().labels().find("project_id") ==
                          ts.metric().labels().end());
              EXPECT_TRUE(ts.metric().labels().find("instance") ==
                          ts.metric().labels().end());
              if (ts.resource().labels().find("table") !=
                  ts.resource().labels().end()) {
                EXPECT_TRUE(ts.metric().labels().find("table") ==
                            ts.metric().labels().end());
                EXPECT_TRUE(ts.metric().labels().find("app_profile") !=
                            ts.metric().labels().end());
                EXPECT_TRUE(ts.metric().labels().find("client_uid") !=
                            ts.metric().labels().end());
              } else {
                EXPECT_TRUE(ts.metric().labels().find("app_profile") ==
                            ts.metric().labels().end());
                EXPECT_TRUE(ts.metric().labels().find("client_uid") ==
                            ts.metric().labels().end());
              }
            }
            return Status();
          });

  auto options =
      Options{}.set<bigtable::MetricsPeriodOption>(std::chrono::seconds(60));
  {
    MetricsOperationContextFactory factory("test-uid", mock_conn, options);
    std::shared_ptr<OperationContext> op = factory.ReadRow(
        "projects/my-project/instances/my-instance/tables/my-table",
        "my-app-profile");
    ASSERT_THAT(op, NotNull());
    op->OnDone(Status());
    op->StubSelection(
        StubSelectionParams{10, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
                            TransportType::kDirectPath, RpcType::kUnary});
  }
}

TEST(MetricsOperationContextFactoryTest, InitializeProviderInstanceLevelRpc) {
  auto mock_conn = std::make_shared<MockMetricServiceConnection>();

  EXPECT_CALL(*mock_conn, CreateServiceTimeSeries)
      .WillRepeatedly(
          [](google::monitoring::v3::CreateTimeSeriesRequest const& request) {
            EXPECT_THAT(request.name(), Eq("projects/my-project"));
            for (auto const& ts : request.time_series()) {
              EXPECT_THAT(ts.resource().type(), Eq("bigtable_client_raw"));
              EXPECT_THAT(ts.resource().labels().at("project_id"),
                          Eq("my-project"));
              EXPECT_THAT(ts.resource().labels().at("instance"),
                          Eq("my-instance"));
            }
            return Status();
          });

  auto options =
      Options{}.set<bigtable::MetricsPeriodOption>(std::chrono::seconds(60));
  {
    MetricsOperationContextFactory factory("test-uid", mock_conn, options);
    std::shared_ptr<OperationContext> op = factory.ExecuteQuery(
        "projects/my-project/instances/my-instance", "my-app-profile");
    ASSERT_THAT(op, NotNull());
    op->OnDone(Status());
  }
}

TEST(MetricsOperationContextFactoryTest, InitializeProviderWithoutConnection) {
  auto options =
      Options{}.set<bigtable::MetricsPeriodOption>(std::chrono::seconds(60));
  MetricsOperationContextFactory factory("test-uid", nullptr, options);

  std::shared_ptr<OperationContext> operation_context = factory.ReadRow(
      "projects/my-project/instances/my-instance/tables/my-table",
      "my-app-profile");
  EXPECT_THAT(operation_context, NotNull());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
