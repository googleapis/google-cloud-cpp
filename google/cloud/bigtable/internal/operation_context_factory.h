// Copyright 2024 Google LLC
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
#include "google/cloud/project.h"
#include "google/cloud/status.h"
#include <grpcpp/grpcpp.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <chrono>
#include <map>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// TODO: Add table_name and app_profile_id parameters for all stub methods.
class OperationContextFactory {
 public:
  virtual ~OperationContextFactory() = 0;
  // ReadRow is a synthetic RPC and should appear in metrics as if it's a
  // different RPC than ReadRows with row_limit=1.
  virtual std::shared_ptr<OperationContext> ReadRow() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> ReadRows() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> AsyncReadRows() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> MutateRow(std::string const&,
                                                      std::string const&) {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> AsyncMutateRow(
      std::string const&, std::string const&) {  // not currently used
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> MutateRows(std::string const&,
                                                       std::string const&) {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> AsyncMutateRows(
      std::string const&, std::string const&) {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> CheckAndMutateRow() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> AsyncCheckAndMutateRow() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> SampleRowKeys() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> AsyncSampleRowKeys() {
    return std::make_shared<OperationContext>();
  }

  virtual std::shared_ptr<OperationContext> ReadModifyWriteRow() {
    return std::make_shared<OperationContext>();
  }
  virtual std::shared_ptr<OperationContext> AsyncReadModifyWriteRow() {
    return std::make_shared<OperationContext>();
  }
};

class SimpleOperationContextFactory : public OperationContextFactory {
 public:
  // ReadRow is a synthetic RPC and should appear in metrics as if it's a
  // different RPC than ReadRows with row_limit=1.
  std::shared_ptr<OperationContext> ReadRow() override {
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> ReadRows() override {
    return std::make_shared<OperationContext>();
  }
  std::shared_ptr<OperationContext> AsyncReadRows() override {
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> MutateRow(std::string const&,
                                              std::string const&) override {
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> AsyncMutateRow(
      std::string const&,
      std::string const&) override {  // not currently used
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> MutateRows(std::string const&,
                                               std::string const&) override {
    return std::make_shared<OperationContext>();
  }
  std::shared_ptr<OperationContext> AsyncMutateRows(
      std::string const&, std::string const&) override {
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> CheckAndMutateRow() override {
    return std::make_shared<OperationContext>();
  }
  std::shared_ptr<OperationContext> AsyncCheckAndMutateRow() override {
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> SampleRowKeys() override {
    return std::make_shared<OperationContext>();
  }
  std::shared_ptr<OperationContext> AsyncSampleRowKeys() override {
    return std::make_shared<OperationContext>();
  }

  std::shared_ptr<OperationContext> ReadModifyWriteRow() override {
    return std::make_shared<OperationContext>();
  }
  std::shared_ptr<OperationContext> AsyncReadModifyWriteRow() override {
    return std::make_shared<OperationContext>();
  }
};

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class MetricsOperationContextFactory : public OperationContextFactory {
 public:
  MetricsOperationContextFactory(Project project, std::string client_uid);

  std::shared_ptr<OperationContext> ReadRow() override;
  std::shared_ptr<OperationContext> ReadRows() override;

  std::shared_ptr<OperationContext> AsyncReadRows() override;

  std::shared_ptr<OperationContext> MutateRow(
      std::string const& table_name,
      std::string const& app_profile_id) override;
  std::shared_ptr<OperationContext> AsyncMutateRow(
      std::string const& table_name,
      std::string const& app_profile_id) override;

  std::shared_ptr<OperationContext> MutateRows(
      std::string const& table_name,
      std::string const& app_profile_id) override;

  std::shared_ptr<OperationContext> AsyncMutateRows(
      std::string const& table_name,
      std::string const& app_profile_id) override;
  std::shared_ptr<OperationContext> CheckAndMutateRow() override;
  std::shared_ptr<OperationContext> AsyncCheckAndMutateRow() override;

  std::shared_ptr<OperationContext> SampleRowKeys() override;
  std::shared_ptr<OperationContext> AsyncSampleRowKeys() override;

  std::shared_ptr<OperationContext> ReadModifyWriteRow() override;
  std::shared_ptr<OperationContext> AsyncReadModifyWriteRow() override;

 private:
  std::shared_ptr<OperationContext::Clock> clock_ =
      std::make_shared<OperationContext::Clock>();
  std::string client_uid_;
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider_;
  std::mutex mu_;  // This is necessary because RPC and AsyncRPC share metrics.

  std::vector<std::shared_ptr<Metric const>>
      mutate_row_metrics_;  // GUARDED_BY(mu_)
  std::vector<std::shared_ptr<Metric const>>
      mutate_rows_metrics_;  // GUARDED_BY(mu_)
  // TODO: add additional Metric vectors for each service RPC.
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_OPERATION_CONTEXT_FACTORY_H
