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
#include "google/cloud/bigtable/internal/metrics.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;

class MockMetric : public Metric {
 public:
  MOCK_METHOD(std::unique_ptr<Metric>, clone, (ResourceLabels, DataLabels),
              (const, override));
};

TEST(MetricsOperationContextFactoryTest, ReadRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const& resource_labels,
                    DataLabels const& data_labels) {
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
  auto operation_context = factory.ReadRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, ReadRows) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const&, DataLabels const& data_labels) {
        EXPECT_THAT(data_labels.method, Eq("ReadRows"));
        EXPECT_THAT(data_labels.streaming, Eq("true"));
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory({}, mock_metric);
  auto operation_context = factory.ReadRows(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, MutateRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const&, DataLabels const& data_labels) {
        EXPECT_THAT(data_labels.method, Eq("MutateRow"));
        EXPECT_THAT(data_labels.streaming, Eq("false"));
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory({}, mock_metric);
  auto operation_context = factory.MutateRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, MutateRows) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const&, DataLabels const& data_labels) {
        EXPECT_THAT(data_labels.method, Eq("MutateRows"));
        EXPECT_THAT(data_labels.streaming, Eq("true"));
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory({}, mock_metric);
  auto operation_context = factory.MutateRows(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, CheckAndMutateRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const&, DataLabels const& data_labels) {
        EXPECT_THAT(data_labels.method, Eq("CheckAndMutateRow"));
        EXPECT_THAT(data_labels.streaming, Eq("false"));
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory({}, mock_metric);
  auto operation_context =
      factory.CheckAndMutateRow(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, SampleRowKeys) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const&, DataLabels const& data_labels) {
        EXPECT_THAT(data_labels.method, Eq("SampleRowKeys"));
        EXPECT_THAT(data_labels.streaming, Eq("true"));
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory({}, mock_metric);
  auto operation_context = factory.SampleRowKeys(table_full_name, app_profile);
}

TEST(MetricsOperationContextFactoryTest, ReadModifyWriteRow) {
  std::string app_profile = "my-app-profile";
  std::string table_full_name =
      "projects/my-project/instances/my-instance/tables/my-table";

  auto mock_metric = std::make_shared<MockMetric const>();
  EXPECT_CALL(*mock_metric, clone)
      .WillOnce([&](ResourceLabels const&, DataLabels const& data_labels) {
        EXPECT_THAT(data_labels.method, Eq("ReadModifyWriteRow"));
        EXPECT_THAT(data_labels.streaming, Eq("false"));
        return std::make_unique<MockMetric>();
      });

  MetricsOperationContextFactory factory({}, mock_metric);
  auto operation_context =
      factory.ReadModifyWriteRow(table_full_name, app_profile);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
