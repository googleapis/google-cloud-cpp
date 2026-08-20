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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_SCHEMA_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_SCHEMA_METRICS_H

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/bigtable/v2/peer_info.pb.h"
#include "google/bigtable/v2/response_params.pb.h"
#include <grpcpp/grpcpp.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct TableResourceLabels {
  std::string project_id;
  std::string instance;
  std::string table;
  std::string cluster;
  std::string zone;
};

struct TableDataLabels {
  std::string method;
  std::string streaming;
  std::string client_name;
  std::string client_uid;
  std::string app_profile;
  std::string status;
};

// Labels populated from the peer info metadata.
struct PeerInfoLabels {
  std::string transport_type;
  std::string transport_region;
  std::string transport_subzone;
};

using LabelMap = std::unordered_map<std::string, std::string>;
// `peer_info_labels` is optional because only AttemptLatency2 populates it.
LabelMap IntoLabelMap(
    TableResourceLabels const& r, TableDataLabels const& d,
    std::set<std::string> const& filtered_data_labels = {},
    std::optional<PeerInfoLabels> const& peer_info_labels = std::nullopt);

bool HasServerTiming(grpc::ClientContext const& client_context);
bool IsConnectivityError(google::cloud::Status const& status,
                         grpc::ClientContext const& client_context);
std::optional<google::bigtable::v2::ResponseParams>
GetResponseParamsFromTrailingMetadata(
    grpc::ClientContext const& client_context);
// Retrieve the peer info from server headers or trailers. Returns nullopt if
// not found or decoding or parsing fails.
std::optional<google::bigtable::v2::PeerInfo> GetPeerInfoFromServerMetadata(
    grpc::ClientContext const& client_context);
std::optional<double> GetServerLatencyFromInitialMetadata(
    grpc::ClientContext const& client_context);

class TableSchemaMetric : public Metric {
 public:
  MetricSchema schema() const final { return MetricSchema::kTable; }
  virtual std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const = 0;
};

class OperationLatency : public TableSchemaMetric {
 public:
  explicit OperationLatency(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;
  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      operation_latencies_;
  OperationContext::Clock::time_point operation_start_;
};

class AttemptLatency : public TableSchemaMetric {
 public:
  AttemptLatency(std::string const& instrumentation_scope,
                 opentelemetry::nostd::shared_ptr<
                     opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      attempt_latencies_;
  OperationContext::Clock::time_point attempt_start_;
};

// Similar to AttemptLatency and also populates the peer info.
class AttemptLatency2 : public TableSchemaMetric {
 public:
  AttemptLatency2(std::string const& instrumentation_scope,
                  opentelemetry::nostd::shared_ptr<
                      opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  PeerInfoLabels peer_info_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      attempt_latencies2_;
  OperationContext::Clock::time_point attempt_start_;
};

class RetryCount : public TableSchemaMetric {
 public:
  RetryCount(std::string const& instrumentation_scope,
             opentelemetry::nostd::shared_ptr<
                 opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const&) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;
  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  std::uint64_t num_retries_ = 0;
  opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      retry_count_;
};

class FirstResponseLatency : public TableSchemaMetric {
 public:
  FirstResponseLatency(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PreCall(opentelemetry::context::Context const&,
               PreCallParams const& p) override;
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void ElementDelivery(opentelemetry::context::Context const&,
                       ElementDeliveryParams const&) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;

  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      first_response_latencies_;
  OperationContext::Clock::time_point operation_start_;
  std::optional<LatencyDuration> first_response_latency_;
};

class ServerLatency : public TableSchemaMetric {
 public:
  ServerLatency(std::string const& instrumentation_scope,
                opentelemetry::nostd::shared_ptr<
                    opentelemetry::metrics::MeterProvider> const& provider);
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;

  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      server_latencies_;
};

class ApplicationBlockingLatency : public TableSchemaMetric {
 public:
  ApplicationBlockingLatency(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PostCall(opentelemetry::context::Context const& context,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void ElementDelivery(opentelemetry::context::Context const&,
                       ElementDeliveryParams const&) override;
  void ElementRequest(opentelemetry::context::Context const&,
                      ElementRequestParams const&) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const& p) override;

  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      application_blocking_latencies_;
  OperationContext::Clock::time_point element_delivery_time_;
  std::vector<LatencyDuration> pending_latencies_;
};

class ConnectivityErrorCount : public TableSchemaMetric {
 public:
  ConnectivityErrorCount(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);
  void PostCall(opentelemetry::context::Context const&,
                grpc::ClientContext const& client_context,
                PostCallParams const& p) override;
  void OnDone(opentelemetry::context::Context const& context,
              OnDoneParams const&) override;
  std::unique_ptr<TableSchemaMetric> clone(
      TableResourceLabels const& resource_labels,
      TableDataLabels const& data_labels) const override;

 private:
  TableResourceLabels resource_labels_;
  TableDataLabels data_labels_;
  std::uint64_t num_errors_ = 0;
  opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::Counter<std::uint64_t>>
      connectivity_error_count_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_SCHEMA_METRICS_H
