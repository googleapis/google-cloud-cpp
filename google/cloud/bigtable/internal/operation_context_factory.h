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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_FACTORY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_FACTORY_H

#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/version.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/monitoring/v3/metric_connection.h"
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class contains methods for each service RPC, each of which, creates a
// OperationContext for use with that RPC. If Metrics are available and enabled,
// the resulting OperationContext contains clones of the Metrics applicable to
// that service RPC.
class OperationContextFactory {
 public:
  virtual ~OperationContextFactory() = 0;
  // ReadRow is a synthetic RPC and should appear in metrics as if it's a
  // different RPC than just ReadRows with row_limit=1.
  virtual std::shared_ptr<OperationContext> ReadRow(
      std::string const& name, std::string const& app_profile);
  virtual std::shared_ptr<OperationContext> ReadRows(
      std::string const& name, std::string const& app_profile);
  virtual std::shared_ptr<OperationContext> MutateRow(
      std::string const& name, std::string const& app_profile);
  virtual std::shared_ptr<OperationContext> MutateRows(
      std::string const& name, std::string const& app_profile);
  virtual std::shared_ptr<OperationContext> CheckAndMutateRow(
      std::string const& name, std::string const& app_profile);
  virtual std::shared_ptr<OperationContext> SampleRowKeys(
      std::string const& name, std::string const& app_profile);
  virtual std::shared_ptr<OperationContext> ReadModifyWriteRow(
      std::string const& name, std::string const& app_profile);
};

class SimpleOperationContextFactory : public OperationContextFactory {
 public:
  std::shared_ptr<OperationContext> ReadRow(
      std::string const& name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> ReadRows(
      std::string const& name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> MutateRow(
      std::string const& name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> MutateRows(
      std::string const& name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> CheckAndMutateRow(
      std::string const& name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> SampleRowKeys(
      std::string const& name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> ReadModifyWriteRow(
      std::string const& name, std::string const& app_profile) override;
};

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class MetricsOperationContextFactory : public OperationContextFactory {
 public:
  explicit MetricsOperationContextFactory(std::string client_uid,
                                          Options options = {});

  // Used for injecting a MockMetricsServiceConnection for testing.
  MetricsOperationContextFactory(
      std::string client_uid,
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      std::shared_ptr<OperationContext::Clock> clock =
          std::make_shared<OperationContext::Clock>(),
      Options options = {});

  // This constructs an instance only suitable for testing. The provided metric
  // is copied into every RPC metric vector, preventing normal Metric
  // initialization and skipping OpenTelemetry provider initialization.
  MetricsOperationContextFactory(std::string client_uid,
                                 std::shared_ptr<Metric const> const& metric);

  std::shared_ptr<OperationContext> ReadRow(
      std::string const& table_name, std::string const& app_profile) override;
  std::shared_ptr<OperationContext> ReadRows(
      std::string const& table_name, std::string const& app_profile) override;

  std::shared_ptr<OperationContext> MutateRow(
      std::string const& table_name, std::string const& app_profile) override;

  std::shared_ptr<OperationContext> MutateRows(
      std::string const& table_name, std::string const& app_profile) override;

  std::shared_ptr<OperationContext> CheckAndMutateRow(
      std::string const& table_name, std::string const& app_profile) override;

  std::shared_ptr<OperationContext> SampleRowKeys(
      std::string const& table_name, std::string const& app_profile) override;

  std::shared_ptr<OperationContext> ReadModifyWriteRow(
      std::string const& table_name, std::string const& app_profile) override;

 private:
  void InitializeProvider(
      std::shared_ptr<monitoring_v3::MetricServiceConnection> conn,
      Options options);

  std::string client_uid_;
  std::shared_ptr<OperationContext::Clock> clock_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;

  // These vectors are initialized exactly once and the initialization is
  // delayed until the first time the corresponding method is called.
  std::vector<std::shared_ptr<Metric const>> read_row_metrics_;
  std::vector<std::shared_ptr<Metric const>> read_rows_metrics_;
  std::vector<std::shared_ptr<Metric const>> mutate_row_metrics_;
  std::vector<std::shared_ptr<Metric const>> mutate_rows_metrics_;
  std::vector<std::shared_ptr<Metric const>> check_and_mutate_row_metrics_;
  std::vector<std::shared_ptr<Metric const>> sample_row_keys_metrics_;
  std::vector<std::shared_ptr<Metric const>> read_modify_write_row_metrics_;
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_FACTORY_H
